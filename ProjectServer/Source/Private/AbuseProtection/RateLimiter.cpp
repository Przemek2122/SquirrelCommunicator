#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"

#include "ThreadCompat.h"

FRateLimit::FRateLimit()
	: AttemptCount(1)
{
}

void FRateLimit::AddAttempt()
{
	AttemptCount++;
}

void FRateLimitObject::AddAttempt(const std::string_view& InKey)
{
	// Shared for reading
	RateLimitMutex.lock_shared();

	const std::unordered_map<std::string_view, FRateLimit>::iterator It = RateLimitMap.find(InKey);
	if (It != RateLimitMap.end())
	{
		FRateLimit& Data = It->second;
		Data.AddAttempt();

		RateLimitMutex.unlock_shared();
	}
	else
	{
		RateLimitMutex.unlock_shared();

		// Exclusive for writing
		std::lock_guard<std::shared_mutex> ScopeLock(RateLimitMutex);

		RateLimitMap[InKey] = FRateLimit();
	}
}

bool FRateLimitObject::IsBlockedKey(const std::string_view& InKey, const int32 NumberOfAttemptsToBlock)
{
	bool bIsBlocked = false;

	const std::unordered_map<std::string_view, FRateLimit>::iterator It = RateLimitMap.find(InKey);
	if (!bIsClearing && It != RateLimitMap.end())
	{
		FRateLimit& Data = It->second;
		if (Data.GetAttemptCount() > NumberOfAttemptsToBlock)
		{
			bIsBlocked = true;
		}
	}

	return bIsBlocked;
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
	const int32 InNumberOfRoomOperationAttemptsToBlock)
	: ClearingTimeInMins(std::chrono::minutes(InClearingTimeInMins))
	, NumberOfAttemptsToBlock(InNumberOfAttemptsToBlock)
	, NumberOfPasswordResetAttemptsToBlock(InNumberOfPasswordResetAttemptsToBlock)
	, NumberOfRoomOperationAttemptsToBlock(InNumberOfRoomOperationAttemptsToBlock)
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

FRateLimiter::~FRateLimiter()
{
	// std::jthread auto-requests stop and joins on destruction
}

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

bool FRateLimiter::IsRoomOperationAddressBlocked(const std::string_view InAddress)
{
	return RoomOperationAddressToLimits.IsBlockedKey(InAddress, NumberOfRoomOperationAttemptsToBlock);
}

void FRateLimiter::AddRoomOperationAttempt(const std::string_view InAddress)
{
	RoomOperationAddressToLimits.AddAttempt(InAddress);
}

void FRateLimiter::ResetRateLimits()
{
	DefaultIPAddressToLimits.Reset();
}
