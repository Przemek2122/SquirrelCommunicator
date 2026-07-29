#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"

#include "ThreadCompat.h"
#include <atomic>
#include <algorithm>

FRateLimit::FRateLimit()
	: AttemptCount(1)
{
}

void FRateLimit::AddAttempt()
{
	// Atomic increment — safe for concurrent calls from multiple threads
	// holding shared_lock on RateLimitMutex.
	AttemptCount.fetch_add(1, std::memory_order_relaxed);
}

void FRateLimitObject::AddAttempt(const std::string_view& InKey)
{
	// First, try to find and increment under shared_lock
	{
		std::shared_lock<std::shared_mutex> Lock(RateLimitMutex);
		auto It = RateLimitMap.find(InKey);
		if (It != RateLimitMap.end())
		{
			// AddAttempt now uses atomic fetch_add — safe under shared_lock
			It->second.AddAttempt();
			return;
		}
	}

	// Key not found in cache — acquire exclusive lock for insertion.
	// Double-check after acquiring: another thread may have inserted
	// the key between the two lock acquisitions (TOCTOU fix).
	{
		std::unique_lock<std::shared_mutex> Lock(RateLimitMutex);
		auto It = RateLimitMap.find(InKey);
		if (It != RateLimitMap.end())
		{
			It->second.AddAttempt();
		}
		else
		{
			// try_emplace constructs FRateLimit in-place (avoids copying std::atomic)
			RateLimitMap.try_emplace(InKey);
		}
	}
}

bool FRateLimitObject::IsBlockedKey(const std::string_view& InKey, const int32 NumberOfAttemptsToBlock)
{
	// Acquire shared_lock for thread-safe read access to RateLimitMap and bIsClearing.
	// This method is called concurrently from REST middleware and WebSocket handlers.
	std::shared_lock<std::shared_mutex> Lock(RateLimitMutex);

	if (bIsClearing)
	{
		return false;
	}

	const auto It = RateLimitMap.find(InKey);
	if (It != RateLimitMap.end())
	{
		const FRateLimit& Data = It->second;
		// GetAttemptCount returns atomic<int32>, load() is safe
		if (Data.GetAttemptCount() > NumberOfAttemptsToBlock)
		{
			return true;
		}
	}

	return false;
}

void FRateLimitObject::Reset()
{
	std::lock_guard<std::mutex> ClearMutexLock(ClearMutex);
	std::lock_guard<std::shared_mutex> RateLimitMutexLock(RateLimitMutex);

	bIsClearing = true;

	RateLimitMap.clear();

	bIsClearing = false;
}

FRateLimiter::FRateLimiter(const int32 InClearingTimeInMins, const int32 InNumberOfAttemptsToBlock, const int32 InNumberOfPasswordResetAttemptsToBlock,
	const int32 InNumberOfServerOperationAttemptsToBlock, const int32 InGlobalRequestsPerHour,
	const int32 InInviteAbuseMaxAttempts, const int32 InInviteAbuseWindowSeconds, const int32 InInviteAbuseBanDurationSeconds)
	: ClearingTimeInMins(std::chrono::minutes(InClearingTimeInMins))
	, NumberOfAttemptsToBlock(InNumberOfAttemptsToBlock)
	, NumberOfPasswordResetAttemptsToBlock(InNumberOfPasswordResetAttemptsToBlock)
	, NumberOfServerOperationAttemptsToBlock(InNumberOfServerOperationAttemptsToBlock)
	, GlobalRequestsPerHour(InGlobalRequestsPerHour)
	, InviteAbuseMaxAttempts(InInviteAbuseMaxAttempts)
	, InviteAbuseWindowSeconds(InInviteAbuseWindowSeconds)
	, InviteAbuseBanDurationSeconds(InInviteAbuseBanDurationSeconds)
{
	AsyncWorkLastTime = std::chrono::utc_clock::now();

	WorkerThread = std::jthread([this](std::stop_token stoken)
	{
		while (!stoken.stop_requested())
		{
			const auto CurrentTime = std::chrono::utc_clock::now();

			if (AsyncWorkLastTime + ClearingTimeInMins <= CurrentTime)
			{
				AsyncWorkLastTime = CurrentTime;
				ResetRateLimits();
			}

			// Sleep 1 second between checks
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});
}

FRateLimiter::~FRateLimiter() = default;

bool FRateLimiter::IsAddressBlocked(const std::string_view InAddress)
{
	return DefaultIPAddressToLimits.IsBlockedKey(InAddress, NumberOfAttemptsToBlock);
}

void FRateLimiter::AddProtectedActionAttempt(const std::string_view InAddress)
{
	DefaultIPAddressToLimits.AddAttempt(InAddress);
}

bool FRateLimiter::IsPasswordResetAddressBlocked(const std::string_view InAddress)
{
	return PasswordResetIPAddressToLimits.IsBlockedKey(InAddress, NumberOfPasswordResetAttemptsToBlock);
}

void FRateLimiter::AddPasswordResetAttempt(const std::string_view InAddress)
{
	PasswordResetIPAddressToLimits.AddAttempt(InAddress);
}

bool FRateLimiter::IsServerOperationAddressBlocked(const std::string_view InAddress)
{
	return ServerOperationAddressToLimits.IsBlockedKey(InAddress, NumberOfServerOperationAttemptsToBlock);
}

void FRateLimiter::AddServerOperationAttempt(const std::string_view InAddress)
{
	ServerOperationAddressToLimits.AddAttempt(InAddress);
}

bool FRateLimiter::IsAddressGloballyBlocked(const std::string_view InAddress)
{
	return GlobalRequestLimits.IsBlockedKey(InAddress, GlobalRequestsPerHour);
}

void FRateLimiter::AddGlobalRequestAttempt(const std::string_view InAddress)
{
	GlobalRequestLimits.AddAttempt(InAddress);
}

void FRateLimiter::ResetRateLimits()
{
	DefaultIPAddressToLimits.Reset();
	PasswordResetIPAddressToLimits.Reset();
	ServerOperationAddressToLimits.Reset();
	GlobalRequestLimits.Reset();

	// NOTE: Invite abuse records are NOT cleared here.
	// They use a rolling-window + ban model with their own independent lifecycle
	// managed by PeriodicInviteAbuseCleanup().
}

// ========== Invite Abuse Protection ==========

Uint64 FRateLimiter::IsInviteAbuseBanned(const std::string& Ip) const
{
	const time_t Now = std::time(nullptr);

	std::shared_lock Lock(InviteAbuseMutex);
	auto Iter = InviteAbuseRecords.find(Ip);
	if (Iter == InviteAbuseRecords.end())
	{
		return 0;
	}

	const FInviteAbuseRecord& Record = Iter->second;
	if (Record.BanUntil > Now)
	{
		return static_cast<Uint64>(Record.BanUntil - Now);
	}

	// Ban has expired — caller can still query, but ban is effectively over.
	// PeriodicInviteAbuseCleanup will remove these.
	return 0;
}

bool FRateLimiter::AddInviteAbuseAttempt(const std::string& Ip)
{
	const time_t Now = std::time(nullptr);

	std::unique_lock Lock(InviteAbuseMutex);
	FInviteAbuseRecord& Record = InviteAbuseRecords[Ip];

	// If currently banned, still count this as another failed attempt
	// (extends the effective ban by keeping the record hot)
	Record.FailedAttempts.push_back(Now);

	// Purge attempts outside the rolling window
	PurgeOldInviteAttempts(Record.FailedAttempts, Now);

	const size_t Count = Record.FailedAttempts.size();

	if (Count >= static_cast<size_t>(InviteAbuseMaxAttempts))
	{
		Record.BanUntil = Now + InviteAbuseBanDurationSeconds;
		return true;
	}

	return false;
}

void FRateLimiter::ClearInviteAbuseForIp(const std::string& Ip)
{
	std::unique_lock Lock(InviteAbuseMutex);
	auto Iter = InviteAbuseRecords.find(Ip);
	if (Iter != InviteAbuseRecords.end())
	{
		InviteAbuseRecords.erase(Iter);
	}
}

void FRateLimiter::PeriodicInviteAbuseCleanup()
{
	const time_t Now = std::time(nullptr);

	std::unique_lock Lock(InviteAbuseMutex);

	// Collect keys to remove
	std::vector<std::string> KeysToRemove;

	for (auto& Pair : InviteAbuseRecords)
	{
		FInviteAbuseRecord& Record = Pair.second;

		// Check if ban has expired
		if (Record.BanUntil > 0 && Record.BanUntil <= Now)
		{
			// Ban expired — mark for removal
			KeysToRemove.push_back(Pair.first);
			continue;
		}

		// Purge old attempts from the rolling window
		PurgeOldInviteAttempts(Record.FailedAttempts, Now);

		// If no attempts remain and no active ban, remove the record entirely
		if (Record.FailedAttempts.empty() && Record.BanUntil == 0)
		{
			KeysToRemove.push_back(Pair.first);
		}
	}

	for (const std::string& Key : KeysToRemove)
	{
		InviteAbuseRecords.erase(Key);
	}
}

void FRateLimiter::PurgeOldInviteAttempts(std::vector<time_t>& Attempts, time_t Now) const
{
	const time_t Cutoff = Now - InviteAbuseWindowSeconds;

	// Remove all attempts older than the cutoff
	Attempts.erase(
		std::remove_if(Attempts.begin(), Attempts.end(),
			[Cutoff](time_t T) { return T < Cutoff; }),
		Attempts.end());
}
