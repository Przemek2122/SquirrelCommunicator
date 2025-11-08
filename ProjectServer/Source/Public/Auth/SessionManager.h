#pragma once

#include "CoreMinimal.h"
#include "Types/Mutex/Mutex.h"

class FGenericThread;

struct FUserSessionData
{
	FUserSessionData();
	FUserSessionData(const Uint64 InUserId, const Uint64 InSessionStartTime);
	FUserSessionData(FUserSessionData& UserSessionData);
	FUserSessionData(FUserSessionData&& UserSessionData) noexcept;

	bool IsValid() const;

	/** User ID which can be used to find correct user */
	Uint64 UserId;

	/** Session start time to know when session should not be alive anymore */
	Uint64 SessionStartTime;

};

class FSessionManager
{
public:
	FSessionManager();
	~FSessionManager();

	void Init();
	void PostSecondTick();

	void AsyncWork();

	/** Single thread to iterate sessions to find which are dead */
	void CheckForDeadSessions();

	std::string CreateSession(const Uint64 InUserId);

	/** Return user ID or 0 on fail */
	Uint64 GetUserIdFromSessionId(const std::string& InSessionToken);

	bool DoesUserHaveSession(const Uint64 InUserId);

	/** @return true if session were found and removed */
	bool DeactivateSession(const std::string& InSessionToken);
	bool IsSessionTokenAlive(const std::string& InSessionToken);

private:
	std::string CreateTokenFromId(const Uint64 InUserId) const;

private:
	/** Session to user Id map */
	CUnorderedMap<std::string, FUserSessionData, Uint64> SessionIdToUserIdMap;

	/** Map with user id to session token mapping */
	CUnorderedMap<Uint64, std::string, Uint64> UserIdToSessionTokenMap;

	/** Mutex for UserDataBase */
	FMutex SessionIdToUserIdMapMutex;

	/** Last updated time in async work */
	Uint64 AsyncWorkLastTime;

	/** Time saved for performance */
	Uint64 CurrentTimeCached;

	/** Key for generating sessions */
	std::string EncryptionKey;

	FThreadData* SessionManagerThreadData;

};
