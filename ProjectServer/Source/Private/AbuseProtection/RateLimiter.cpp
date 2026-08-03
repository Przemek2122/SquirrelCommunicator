#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"

#include "ThreadCompat.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <eve/module/algo.hpp>

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
			// AddAttempt uses atomic fetch_add — safe under shared_lock
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
			// Construct std::string from string_view for safe storage.
			// try_emplace constructs the key+value in-place (avoids copying std::atomic).
			RateLimitMap.try_emplace(std::string(InKey));
		}
	}
}

bool FRateLimitObject::IsBlockedKey(const std::string_view& InKey, const int32 NumberOfAttemptsToBlock)
{
	// Acquire shared_lock for thread-safe read access to RateLimitMap.
	// This method is called concurrently from REST middleware and WebSocket handlers.
	std::shared_lock<std::shared_mutex> Lock(RateLimitMutex);

	// Transparent lookup: find(string_view) on map<string,...> with
	// FTransparentStringHash/FTransparentStringEqual = ZERO allocation.
	const auto It = RateLimitMap.find(InKey);
	if (It != RateLimitMap.end())
	{
		const FRateLimit& Data = It->second;
		if (Data.GetAttemptCount() > NumberOfAttemptsToBlock)
		{
			return true;
		}
	}

	return false;
}

void FRateLimitObject::Reset()
{
	// Exclusive lock on RateLimitMutex is sufficient — no other thread
	// can access the map concurrently during reset. No separate ClearMutex needed.
	std::unique_lock<std::shared_mutex> Lock(RateLimitMutex);
	RateLimitMap.clear();
}

FRateLimiter::FRateLimiter(const int32 InClearingTimeInMins, const int32 InNumberOfAttemptsToBlock, const int32 InNumberOfPasswordResetAttemptsToBlock,
	const int32 InNumberOfServerOperationAttemptsToBlock,
	const int32 InUnauthenticatedRequestsPerHour, const int32 InAuthenticatedRequestsPerHour,
	const int32 InInviteAbuseMaxAttempts, const int32 InInviteAbuseWindowSeconds, const int32 InInviteAbuseBanDurationSeconds,
	const int32 InInviteCreateLimitPerHour, const int32 InInviteUseLimitPerHour,
	const int32 InRegisterAccountLimitPerHour)
	: ClearingTimeInMins(std::chrono::minutes(InClearingTimeInMins))
	, NumberOfAttemptsToBlock(InNumberOfAttemptsToBlock)
	, NumberOfPasswordResetAttemptsToBlock(InNumberOfPasswordResetAttemptsToBlock)
	, NumberOfServerOperationAttemptsToBlock(InNumberOfServerOperationAttemptsToBlock)
	, UnauthenticatedRequestsPerHour(InUnauthenticatedRequestsPerHour)
	, AuthenticatedRequestsPerHour(InAuthenticatedRequestsPerHour)
	, InviteCreateLimitPerHour(InInviteCreateLimitPerHour)
	, InviteUseLimitPerHour(InInviteUseLimitPerHour)
	, RegisterAccountLimitPerHour(InRegisterAccountLimitPerHour)
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

// --- Two-tier global rate limiting ---

bool FRateLimiter::IsUnauthenticatedIPBlocked(const std::string_view InAddress)
{
	return UnauthenticatedIPLimits.IsBlockedKey(InAddress, UnauthenticatedRequestsPerHour);
}

void FRateLimiter::AddUnauthenticatedIPAttempt(const std::string_view InAddress)
{
	UnauthenticatedIPLimits.AddAttempt(InAddress);
}

bool FRateLimiter::IsAuthenticatedUserBlocked(const std::string_view UserIdStr)
{
	return AuthenticatedUserLimits.IsBlockedKey(UserIdStr, AuthenticatedRequestsPerHour);
}

void FRateLimiter::AddAuthenticatedUserAttempt(const std::string_view UserIdStr)
{
	AuthenticatedUserLimits.AddAttempt(UserIdStr);
}

// --- Invite create hourly rate limit ---

bool FRateLimiter::IsInviteCreateAddressBlocked(const std::string_view InAddress)
{
	return InviteCreateLimits.IsBlockedKey(InAddress, InviteCreateLimitPerHour);
}

void FRateLimiter::AddInviteCreateAttempt(const std::string_view InAddress)
{
	InviteCreateLimits.AddAttempt(InAddress);
}

// --- Invite use hourly rate limit ---

bool FRateLimiter::IsInviteUseAddressBlocked(const std::string_view InAddress)
{
	return InviteUseLimits.IsBlockedKey(InAddress, InviteUseLimitPerHour);
}

void FRateLimiter::AddInviteUseAttempt(const std::string_view InAddress)
{
	InviteUseLimits.AddAttempt(InAddress);
}

// --- Registration rate limiting ---

bool FRateLimiter::IsRegisterAccountAddressBlocked(const std::string_view InAddress)
{
	return RegisterAccountLimits.IsBlockedKey(InAddress, RegisterAccountLimitPerHour);
}

void FRateLimiter::AddRegisterAccountAttempt(const std::string_view InAddress)
{
	RegisterAccountLimits.AddAttempt(InAddress);
}

void FRateLimiter::ResetRateLimits()
{
	DefaultIPAddressToLimits.Reset();
	PasswordResetIPAddressToLimits.Reset();
	ServerOperationAddressToLimits.Reset();
	UnauthenticatedIPLimits.Reset();
	AuthenticatedUserLimits.Reset();
	InviteCreateLimits.Reset();
	InviteUseLimits.Reset();
	RegisterAccountLimits.Reset();

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

	// Collect keys to remove. Pre-allocate for typical cleanup ratio.
	std::vector<std::string> KeysToRemove;
	KeysToRemove.reserve(InviteAbuseRecords.size() / 4 + 1);

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

	// EVE SIMD remove_if — same C++ code, portable across all architectures.
	// The erase-remove idiom matches std::remove_if semantics exactly:
	//   - Elements where predicate returns true are "removed" (moved to end)
	//   - Returns iterator to new logical end
	//   - vector::erase shrinks to the kept elements
	Attempts.erase(
		eve::algo::remove_if(
			Attempts,
			[Cutoff](auto t) { return t < Cutoff; }
		),
		Attempts.end()
	);
}
