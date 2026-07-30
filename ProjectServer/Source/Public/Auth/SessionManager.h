// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"
#include <shared_mutex>
#include <thread>

struct FUserSessionData
{
	FUserSessionData();
	FUserSessionData(Uint64 InUserId, Uint64 InSessionTime);
	FUserSessionData(const FUserSessionData& UserSessionData);
	FUserSessionData(FUserSessionData&& UserSessionData) noexcept;

	bool IsValid() const;

	void SetSessionTimeLeft(const Uint64 InSessionTime);
	Uint64 GetSessionTime() const;

	/** User ID which can be used to find correct user */
	Uint64 UserId;

private:
	/** Session time to know when session should not be alive anymore */
	Uint64 SessionTime;

	/** Mutex for updating session */
	std::shared_mutex SessionUpdateMutex;

};

class FSessionManager
{
public:
	FSessionManager(Uint64 InSessionExpirationTime);
	~FSessionManager() = default;

	void Init();
	void PostSecondTick();

	void AsyncCheckForDeadSessions();

	std::string CreateSession(Uint64 InUserId);

	/** Return user ID or 0 on fail */
	Uint64 GetUserIdFromSessionId(const std::string& InSessionToken);

	/** Refreshes session token by changing its internal time */
	bool RefreshSessionToken(const std::string& InSessionToken);

	bool DoesUserHaveSession(Uint64 InUserId);

	/** @return true if session were found and removed */
	bool DeactivateSession(const std::string& InSessionToken);
	bool IsSessionTokenAlive(const std::string& InSessionToken);

private:
	std::string CreateTokenFromId(Uint64 InUserId) const;

private:
	/** Session to user Id map */
	CUnorderedMap<std::string, FUserSessionData, Uint64> SessionIdToUserIdMap;

	/** Map with user id to session token mapping */
	CUnorderedMap<Uint64, std::string, Uint64> UserIdToSessionTokenMap;

	/** Mutex for UserDataBase */
	std::shared_mutex SessionIdToUserIdMapMutex;

	/** Last updated time in async work */
	Uint64 AsyncWorkLastTime;

	/** Time saved for performance */
	Uint64 CurrentTimeCached;

	/** Session expiration time in seconds */
	Uint64 SessionExpirationTime;

	/** Background worker thread */
	std::jthread WorkerThread;

};
