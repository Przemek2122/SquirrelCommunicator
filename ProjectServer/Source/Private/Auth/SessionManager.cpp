#include "Auth/SessionManager.h"

#include "Misc/EncryptionUtil.h"
#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"
#include "Types/Mutex/MutexScopeLock.h"

static const char* SessionManagerThreadName = "SessionManagerThread";

FUserSessionData::FUserSessionData()
	: UserId(0)
	, SessionStartTime(0)
{
}

FUserSessionData::FUserSessionData(const Uint64 InUserId, const Uint64 InSessionStartTime)
	: UserId(InUserId)
	, SessionStartTime(InSessionStartTime)
{
}

FUserSessionData::FUserSessionData(FUserSessionData& UserSessionData)
	: UserId(UserSessionData.UserId)
	, SessionStartTime(UserSessionData.SessionStartTime)
{
}

FUserSessionData::FUserSessionData(FUserSessionData&& UserSessionData) noexcept
	: UserId(UserSessionData.UserId)
	, SessionStartTime(UserSessionData.SessionStartTime)
{
}

bool FUserSessionData::IsValid() const
{
	return (UserId != 0 && SessionStartTime != 0);
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

		GenericThread->BeginAsyncWork();
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
		THREAD_WAIT_MS(1);
	}
	else
	{
		AsyncWorkLastTime = CurrentTime;

		CheckForDeadSessions();
	}
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

Uint64 FSessionManager::GetUserIdFromSessionId(const std::string& InSessionToken)
{
	Uint64 OutId = 0;

	if (SessionIdToUserIdMap.ContainsKey(InSessionToken))
	{
		FUserSessionData& SessionData = SessionIdToUserIdMap[InSessionToken];

		if (IsSessionTokenAlive(InSessionToken))
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
		bIsSessionTokenAlive = true;
	}

	return bIsSessionTokenAlive;
}

std::string FSessionManager::CreateTokenFromId(const Uint64 InUserId) const
{
	std::string OutSession;
	std::string TemporaryString;

	// Add salt
	const std::string Salt = FEncryptionUtil::GenerateSecureSalt(64);
	const std::string SaltAsBase62 = FEncryptionUtil::ToBaseN_Irreversible(Salt, FPredefinedCharsets::BASE62);
	TemporaryString += SaltAsBase62;

	const std::string Salt2 = FEncryptionUtil::GenerateSecureSalt(64);
	const std::string Salt2AsBase62 = FEncryptionUtil::ToBaseN_Irreversible(Salt2, FPredefinedCharsets::BASE62);

	static constexpr uint64_t SessionFlipMask = 0x9E3779B97F4A7C15ULL;
	const Uint64 FlippedNumber = FBitFlipping::FlipBits(InUserId, SessionFlipMask);
	const std::string NumberAsBase62 = FEncryptionUtil::ToBaseNNum(FlippedNumber, FPredefinedCharsets::BASE62);

	TemporaryString += NumberAsBase62;
	TemporaryString += Salt2AsBase62;

	const std::string EncryptedData = FEncryptionUtil::EncryptDataCustom(TemporaryString, EncryptionKey);
	const std::string SimpleData = FEncryptionUtil::ToBaseN_Irreversible(EncryptedData, FPredefinedCharsets::BASE62);

	return SimpleData;
}
