#pragma once

#include "CoreMinimal.h"
#include "Types/Mutex/Mutex.h"

class FGenericThread;

struct FUserSessionData
{
	FUserSessionData(const Uint64 InUserId, const Uint64 InSessionStartTime);

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
	std::string CreateTokenFromId(const Uint64 InUserId);
	Uint64 GetUserIdFromSessionId(const std::string& InSessionToken);

	void DeactivateSession(const std::string& InSessionToken);
	bool IsSessionTokenAlive(const std::string& InSessionToken);

	void Save();
	void Load();

private:
	/** Session to user Id map */
	CUnorderedMap<std::string, FUserSessionData, Uint64> SessionIdToUserIdMap;

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
