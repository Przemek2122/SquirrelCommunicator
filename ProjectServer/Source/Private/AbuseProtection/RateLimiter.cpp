#include "ProjectEngine.h"
#include "AbuseProtection/RateLimiter.h"

#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"

static const char* RateLimiterThreadName = "RateLimiterThread";

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

void FRateLimiter::AsyncWork()
{
	const std::chrono::time_point<std::chrono::utc_clock> CurrentTime = std::chrono::utc_clock::now();

	if (AsyncWorkLastTime + ClearingTimeInMins > CurrentTime)
	{
		THREAD_WAIT_MS(1);
	}
	else
	{
		AsyncWorkLastTime = CurrentTime;

		ResetRateLimits();
	}
}

void FRateLimiter::ResetRateLimits()
{
	DefaultIPAddressToLimits.Reset();
}
