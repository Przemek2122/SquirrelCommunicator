#include "SessionManager.h"
#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"

FUserSessionData::FUserSessionData(const Uint64 InUserId)
	: UserId(InUserId)
	, SessionStartTime(0)
{
}

FSessionManager::FSessionManager()
	: AsyncWorkLastTime(0)
{
}

void FSessionManager::Init()
{
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	FThreadData* CheckForDeadSessionsThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>("SessionManagerThread");
	FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(CheckForDeadSessionsThreadData->GetThread());
	if (GenericThread != nullptr)
	{
		GenericThread->AddTask([this]()
		{
			AsyncWork();
		});
	}
}

void FSessionManager::AsyncWork()
{
	constexpr Uint64 TimeBetweenRuns = 3 * 60; // Time in seconds
	const Uint64 CurrentTime = FUtil::GetSeconds();

	if (AsyncWorkLastTime + TimeBetweenRuns > CurrentTime)
	{
		const Uint64 TimeToWait = FUtil::SecondToMilliSecond(CurrentTime - AsyncWorkLastTime + TimeBetweenRuns);
		THREAD_WAIT_MS(TimeToWait);
	}

	AsyncWorkLastTime = CurrentTime;

	CheckForDeadSessions();
}

void FSessionManager::CheckForDeadSessions()
{
	for (std::pair<const std::string, FUserSessionData>& Session : SessionIdToUserIdMap)
	{
		if (!IsSessionTokenAlive(Session.first))
		{
			DeactivateSession(Session.first);
		}
	}
}

std::string FSessionManager::CreateSessionFromToken(const Uint64 InUserId)
{
	std::string OutSession;

	// Add salt
	OutSession += FUtil::GenerateSecureSalt(32);

	static constexpr uint64_t SessionFlipMask = 0x9E3779B97F4A7C15ULL;
	const Uint64 FlippedNumber = FUtil::FlipBits(InUserId, SessionFlipMask);
	const std::string NumberAsBase62 = FUtil::ToBaseN(FlippedNumber);

	// Add id as something that will be potentialy not as easy to read as number
	OutSession += NumberAsBase62;

	return OutSession;
}

Uint64 FSessionManager::GetUserIdFromSessionId(const std::string& InSessionToken)
{
	Uint64 OutId = 0;

	for (auto& [SessionName, SessionData] : SessionIdToUserIdMap)
	{
		if (SessionName == InSessionToken && !IsSessionTokenAlive(SessionName))
		{
			OutId = SessionData.UserId;
		}
	}

	return OutId;
}

void FSessionManager::DeactivateSession(const std::string& InSessionToken)
{
	SessionIdToUserIdMap.Remove(InSessionToken);
}

bool FSessionManager::IsSessionTokenAlive(const std::string& InSessionToken)
{
	bool bIsSessionTokenAlive = false;

	if (SessionIdToUserIdMap.ContainsKey(InSessionToken))
	{
		const FUserSessionData Value = SessionIdToUserIdMap.FindValueByKey(InSessionToken);

		bIsSessionTokenAlive = true;
	}

	return bIsSessionTokenAlive;
}
