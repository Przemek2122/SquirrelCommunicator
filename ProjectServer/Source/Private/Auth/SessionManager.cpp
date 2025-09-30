#include "Auth/SessionManager.h"

#include "Misc/EncryptionUtil.h"
#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"
#include "Types/Mutex/MutexScopeLock.h"

static const char* SessionManagerThreadName = "SessionManagerThread";

FUserSessionData::FUserSessionData(const Uint64 InUserId, const Uint64 InSessionStartTime)
	: UserId(InUserId)
	, SessionStartTime(InSessionStartTime)
{
}

FSessionManager::FSessionManager()
	: AsyncWorkLastTime(0)
	, CurrentTimeCached(0)
	, SessionManagerThreadData(nullptr)
{
}

FSessionManager::~FSessionManager()
{
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	ThreadsManager->TryStopThread(SessionManagerThreadData);
}

void FSessionManager::Init()
{
	CurrentTimeCached = FUtil::GetSeconds();

	if (EncryptionKey.empty())
	{
		EncryptionKey = FEncryptionUtil::GenerateSecureSalt(64);
	}

	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	SessionManagerThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>(SessionManagerThreadName);
	FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(SessionManagerThreadData->GetThread());
	GenericThread->SetShouldRemoveDoneJobs(false);
	if (GenericThread != nullptr)
	{
		GenericThread->AddTask([this]()
		{
			AsyncWork();
		});
	}
}

void FSessionManager::PostSecondTick()
{
	CurrentTimeCached = FUtil::GetSeconds();
}

void FSessionManager::AsyncWork()
{
	constexpr Uint64 TimeBetweenRuns = 3 * 60; // Time in seconds
	const Uint64 CurrentTime = CurrentTimeCached;

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

std::string FSessionManager::CreateSession(const Uint64 InUserId)
{
	const std::string SessionToken = CreateTokenFromId(InUserId);
	FUserSessionData UserSessionData(InUserId, CurrentTimeCached);

	FMutexScopeLock MutexScopeLock(SessionIdToUserIdMapMutex);
	SessionIdToUserIdMap.Emplace(SessionToken, UserSessionData);
	UserIdToSessionTokenMap.Emplace(InUserId, SessionToken);

	return SessionToken;
}

std::string FSessionManager::CreateTokenFromId(const Uint64 InUserId) const 
{
	std::string OutSession;

	// Add salt
	const std::string Salt = FEncryptionUtil::GenerateSecureSalt(128);
	const std::string SaltAsBase62 = FEncryptionUtil::ToBaseN_Irreversible(Salt, FEncryptionUtil::PREDEFINED_CHARACTERSET_BASE62);
	OutSession += SaltAsBase62;

	static constexpr uint64_t SessionFlipMask = 0x9E3779B97F4A7C15ULL;
	const Uint64 FlippedNumber = FEncryptionUtil::FlipBits(InUserId, SessionFlipMask);
	const std::string NumberAsBase62 = FEncryptionUtil::ToBaseNNum(FlippedNumber, FEncryptionUtil::PREDEFINED_CHARACTERSET_BASE62);

	const std::string Encrypted = FEncryptionUtil::EncryptCustomBaseValidated(NumberAsBase62, FEncryptionUtil::PREDEFINED_CHARACTERSET_BASE62, EncryptionKey, true);

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

bool FSessionManager::DoesUserHaveSession(const Uint64 InUserId)
{
	return UserIdToSessionTokenMap.ContainsKey(InUserId);
}

bool FSessionManager::DeactivateSession(const std::string& InSessionToken)
{
	bool bDeactivatedSession = false;

	if (SessionIdToUserIdMap.ContainsKey(InSessionToken))
	{
		{
			const FUserSessionData& UserSessionData = SessionIdToUserIdMap.FindValueByKey(InSessionToken);
			const Uint64 UserId = UserSessionData.UserId;
			bDeactivatedSession = UserIdToSessionTokenMap.Remove(UserId);
		}

		SessionIdToUserIdMap.Remove(InSessionToken);
	}

	return bDeactivatedSession;
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

void FSessionManager::Save()
{
	// Save EncryptionKey
}

void FSessionManager::Load()
{
	// Load EncryptionKey
}
