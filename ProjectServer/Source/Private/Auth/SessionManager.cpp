// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Auth/SessionManager.h"

#include "Misc/EncryptionUtil.h"
#include "Threads/Thread.h"
#include "Threads/ThreadsManager.h"

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

void FUserSessionData::SetSessionStartTime(Uint64 InSessionStartTime)
{
	std::unique_lock MutexScopeLock(SessionUpdateMutex);

	SessionStartTime = InSessionStartTime;
}

Uint64 FUserSessionData::GetSessionStartTime() const
{
	return SessionStartTime;
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
	if (GenericThread != nullptr)
	{
		GenericThread->SetShouldRemoveDoneJobs(false);
		GenericThread->AddTask([this]()
		{
			AsyncWork();
		});

		GenericThread->BeginAsyncWork();
	}
	else
	{
		LOG_ERROR("Failed to create session manager thread");
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
		THREAD_WAIT_MS(1000);
	}
	else
	{
		AsyncWorkLastTime = CurrentTime;

		AsyncCheckForDeadSessions();
	}
}

void FSessionManager::AsyncCheckForDeadSessions()
{
	// @TODO: In future it would be smart to implement priority_queue as it would be O(1)

	std::vector<std::string> SessionIds;

	// Make copy
	{
		std::shared_lock ReadLock(SessionIdToUserIdMapMutex);

		SessionIds.reserve(SessionIdToUserIdMap.Size());

		for (std::pair<const std::string, FUserSessionData>& Session : SessionIdToUserIdMap)
		{
			SessionIds.push_back(Session.first);
		}
	}

	for (const std::string& SessionId : SessionIds)
	{
		if (!IsSessionTokenAlive(SessionId))
		{
			DeactivateSession(SessionId);
		}
	}
}

std::string FSessionManager::CreateSession(const Uint64 InUserId)
{
	const std::string SessionToken = CreateTokenFromId(InUserId);
	FUserSessionData UserSessionData(InUserId, CurrentTimeCached);

	std::lock_guard<std::shared_mutex> MutexScopeLock(SessionIdToUserIdMapMutex);
	SessionIdToUserIdMap.Emplace(SessionToken, UserSessionData);
	UserIdToSessionTokenMap.Emplace(InUserId, SessionToken);

	return SessionToken;
}

Uint64 FSessionManager::GetUserIdFromSessionId(const std::string& InSessionToken)
{
	Uint64 OutId = 0;

	bool bSessionExists = false;

	{
		std::shared_lock<std::shared_mutex> ReadLock(SessionIdToUserIdMapMutex);
		bSessionExists = SessionIdToUserIdMap.ContainsKey(InSessionToken);
	}

	if (bSessionExists && IsSessionTokenAlive(InSessionToken))
	{
		std::shared_lock<std::shared_mutex> ReadLock(SessionIdToUserIdMapMutex);
		FUserSessionData SessionData = SessionIdToUserIdMap[InSessionToken];

		OutId = SessionData.UserId;
	}

	return OutId;
}

bool FSessionManager::RefreshSessionToken(const std::string& InSessionToken)
{
	bool bIsAlive = false;

	bool bSessionExists = false;
	{
		std::shared_lock ReadLock(SessionIdToUserIdMapMutex);
		bSessionExists = SessionIdToUserIdMap.ContainsKey(InSessionToken);
	}

	if (bSessionExists)
	{
		if (IsSessionTokenAlive(InSessionToken))
		{
			std::lock_guard<std::shared_mutex> MutexScopeLock(SessionIdToUserIdMapMutex);
			FUserSessionData& SessionData = SessionIdToUserIdMap[InSessionToken];
			SessionData.SetSessionStartTime(CurrentTimeCached);

			bIsAlive = true;
		}
	}

	return bIsAlive;
}

bool FSessionManager::DoesUserHaveSession(const Uint64 InUserId)
{
	return UserIdToSessionTokenMap.ContainsKey(InUserId);
}

bool FSessionManager::DeactivateSession(const std::string& InSessionToken)
{
	bool bDeactivatedSession = false;

	bool bSessionExists = false;
	{
		std::shared_lock ReadLock(SessionIdToUserIdMapMutex);
		bSessionExists = SessionIdToUserIdMap.ContainsKey(InSessionToken);
	}

	if (bSessionExists)
	{
		{
			const std::optional<FUserSessionData> UserSessionData = SessionIdToUserIdMap.FindValueByKey(InSessionToken);
			if (UserSessionData.has_value())
			{
				const Uint64 UserId = UserSessionData->UserId;
				bDeactivatedSession = UserIdToSessionTokenMap.Remove(UserId);
			}
		}

		SessionIdToUserIdMap.Remove(InSessionToken);
	}

	return bDeactivatedSession;
}

bool FSessionManager::IsSessionTokenAlive(const std::string& InSessionToken)
{
	bool bIsSessionTokenAlive = false;

	bool bSessionExists = false;
	{
		std::shared_lock ReadLock(SessionIdToUserIdMapMutex);
		bSessionExists = SessionIdToUserIdMap.ContainsKey(InSessionToken);
	}

	if (bSessionExists)
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
