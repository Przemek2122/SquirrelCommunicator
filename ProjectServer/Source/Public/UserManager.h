#pragma once

#include "CoreMinimal.h"

class FUserManager;

enum class FUserStatus
{
	Unknown,
	Online,
	Offline
};

/** Class for single user */
class FUser
{
public:
	FUser(FUserManager* InUserManager);

	/** Update last active time with current time */
	void UpdateLastActiveTime();

	bool IsUserNameCorrect(const std::string& InUserName) const;
	bool IsUserPasswordCorrect(const std::string& InUserPassword) const;

	/** @return Name displayed for other users */
	const std::string& GetDisplayedName() const;

	/** @return User status depending on last time active */
	FUserStatus GetUserStatus() const;

protected:
	Uint64 GetCurrentTime() const;

private:
	/** Displayed name to other users */
	std::string DisplayedName;

	/** Username (for log in) */
	std::string UserName;

	/** User password (for log in) */
	std::string UserPassword;

	/** Timestamp of last activity */
	Uint64 LastActiveTime;

	/** User manager class */
	FUserManager* UserManager;
	
};

/** Class for managing users */
class FUserManager
{
public:
	/** Updated every second */
	void PostSecondTick();

	bool DoesUserExist(const std::string& InUserName);
	bool AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword);
	//bool AddUser();

	Uint64 GetCurrentTimeCached() const { return CurrentTimeCached; }

	/** @TODO Implement some kind of database for users (for saving / loading) (or use existing one) */

private:
	/** Array with all users */
	std::vector<FUser> UserDataBase;

	/** Cache for time, we will use it for each login, message, etc so cache will be faster */
	Uint64 CurrentTimeCached;

};
