#pragma once

#include "CoreMinimal.h"

struct FUserSessionData
{
	FUserSessionData(const Uint64 InUserId);

	/** User ID which can be used to find correct user */
	Uint64 UserId;

	/** Session start time to know when session should not be alive anymore */
	Uint64 SessionStartTime;

};

class FSessionManager
{
public:
	FSessionManager();

	void Init();

	void AsyncWork();

	/** Single thread to iterate sessions to find which are dead */
	void CheckForDeadSessions();

	std::string CreateSessionFromToken(const Uint64 InUserId);
	Uint64 GetUserIdFromSessionId(const std::string& InSessionToken);

	void DeactivateSession(const std::string& InSessionToken);
	bool IsSessionTokenAlive(const std::string& InSessionToken);

private:
	/** Session to user Id map */
	CUnorderedMap<std::string, FUserSessionData, Uint64> SessionIdToUserIdMap;

	/** Last updated time in async work */
	Uint64 AsyncWorkLastTime;

};
