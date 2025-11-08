#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"

#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"
#include "Types/Mutex/MutexScopeLock.h"

static const char* RateLimiterThreadName = "RateLimiterThread";

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

void FRateLimitObject::Reset()
{
	FMutexScopeLock ClearMutexScopeLock(ClearMutex);
	FMutexScopeLock RateLimitMutexScopeLock(RateLimitMutex);

	RateLimitMap.clear();
}

FRateLimiter::FRateLimiter(const int32 InClearingTimeInMins, const int32 InNumberOfAttemptsToBlock)
	: ClearingTimeInMins(std::chrono::minutes(InClearingTimeInMins))
	, NumberOfAttemptsToBlock(InNumberOfAttemptsToBlock)
{
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	RateLimiterThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>(RateLimiterThreadName);
	FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(RateLimiterThreadData->GetThread());
	GenericThread->SetShouldRemoveDoneJobs(false);
	if (GenericThread != nullptr)
	{
		AsyncWorkLastTime = std::chrono::utc_clock::now();

		GenericThread->AddTask([this]()
		{
			AsyncWork();
		});

		GenericThread->BeginAsyncWork();
	}
}

FRateLimiter::~FRateLimiter()
{
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	ThreadsManager->TryStopThread(RateLimiterThreadData);
}

bool FRateLimiter::IsAddressBlocked(const std::string& InAddress)
{
	return IPAddressToLimits.IsBlockedKey(InAddress, NumberOfAttemptsToBlock);
}

void FRateLimiter::AddProtectedActionAttempt(const std::string& InAddress)
{
	IPAddressToLimits.AddAttempt(InAddress);
}

void FRateLimiter::AsyncWork()
{
	const std::chrono::time_point<std::chrono::utc_clock> CurrentTime = std::chrono::utc_clock::now();

	if (AsyncWorkLastTime + ClearingTimeInMins > CurrentTime)
	{
		THREAD_WAIT_SHORT_TIME;
	}
	else
	{
		AsyncWorkLastTime = CurrentTime;

		ResetRateLimits();
	}
}

void FRateLimiter::ResetRateLimits()
{
	IPAddressToLimits.Reset();
}
