#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"
#include "Types/Mutex/MutexScopeLock.h"

FRateLimit::FRateLimit()
	: AttemptCount(1)
{
}

void FRateLimit::AddAttempt()
{
	AttemptCount++;
}

void FRateLimitObject::AddAttempt(const std::string& InKey)
{
	auto It = RateLimitMap.find(InKey);
	if (It != RateLimitMap.end())
	{
		FMutexScopeLock ScopeLock(RateLimitMutex);

		FRateLimit& Data = It->second; 
		Data.AddAttempt();
	}
	else
	{
		FMutexScopeLock ScopeLock(RateLimitMutex);

		RateLimitMap[InKey] = FRateLimit();
	}
}

bool FRateLimitObject::IsBlockedKey(const std::string& InKey, const int32 NumberOfAttemptsToBlock)
{
	bool bIsBlocked = false;

	std::unordered_map<std::string, FRateLimit>::iterator It = RateLimitMap.find(InKey);
	if (!ClearMutex.IsLocked() && It != RateLimitMap.end())
	{
		FRateLimit& Data = It->second;
		if (Data.GetAttemptCount() > NumberOfAttemptsToBlock)
		{
			bIsBlocked = true;
		}
	}

	return bIsBlocked;
}

FRateLimiter::FRateLimiter(const int32 InClearTime, const int32 InNumberOfAttemptsToBlock)
	: ClearTime(InClearTime)
	, NumberOfAttemptsToBlock(InNumberOfAttemptsToBlock)
{
}

FRateLimiter::~FRateLimiter()
{
}

bool FRateLimiter::IsAddressBlocked(const std::string& InAddress)
{
	return IPAddressToLimits.IsBlockedKey(InAddress, NumberOfAttemptsToBlock);
}

void FRateLimiter::AddProtectedActionAttempt(const std::string& InAddress)
{
	IPAddressToLimits.AddAttempt(InAddress);
}
