#pragma once

#include "CoreMinimal.h"
#include "SessionManager.h"
#include "User.h"
#include "Types/Mutex/Mutex.h"

enum class ERegisterUserStatus : Uint8
{
	Unknown,
	Successful,
	LoginTaken
};

enum class ELoginStatus : Uint8
{
	Unknown,
	Successful,
	SessionAlreadyExist,
	IncorrectCredentialsOrUserDoesNotExist
};

/** Class for managing users */
class FUserManager
{
public:
	FUserManager();

	void Init();

	/** Updated every second */
	void PostSecondTick();

	/**
	 * Use to register
	 * @return registration status, see enum for details
	 */
	ERegisterUserStatus RegisterUser(const std::string& InUserName, const std::string& InUserPassword, const std::string& InUserEMail);

	/**
	 * Use for login
	 * @return Unique session token
	 */
	ELoginStatus LoginUser(const std::string& InUserName, const std::string& InUserPassword, std::string& OutSessionToken);

	bool DoesUserExist(const std::string& InUserName);
	bool AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword);

	Uint64 GetCurrentTimeCached() const { return CurrentTimeCached; }

	/** @TODO Implement some kind of database for users (for saving / loading) (or use existing one) */

	void LoadUsers();
	void SaveUsers();
	void SaveUsersBackup();

protected:
	Uint64 GenerateNextAvailableId();

private:
	/** Manager for user sessions */
	std::unique_ptr<FSessionManager> SessionManager;

	/** Map with all users connected to number */
	CUnorderedMap<Uint64, std::shared_ptr<FUser>, Uint64> UserDataBase;

	/** Tells us which number is available */
	Uint64 NextAvailableIndex;

	/** Mutex for UserDataBase */
	FMutex UserDataBaseMutex;

	/** Cache for time, we will use it for each login, message, etc so cache will be faster */
	Uint64 CurrentTimeCached;

};
