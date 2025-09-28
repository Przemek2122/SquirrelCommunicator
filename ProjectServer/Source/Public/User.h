#pragma once

#include "CoreMinimal.h"

class FUserManager;

enum class EUserStatus : Uint8
{
	Unknown,
	Online,
	Offline
};

enum class ERegisterUserStatus : Uint8
{
	Unknown,
	Successful,
	LoginTaken
};

/** Class for single user */
class FUser
{
public:
	FUser(FUserManager* InUserManager = nullptr);

	/** Update last active time with current time */
	void UpdateLastActiveTime();

	void SetDisplayedName(const std::string& InDisplayedName);
	void SetUserName(const std::string& InUserName);
	void SetPassword(const std::string& InUserPassword);
	void SetUserEMail(const std::string& InUserEMail);

	bool IsUserNameCorrect(const std::string& InUserName) const;
	bool IsUserPasswordCorrect(const std::string& InUserPassword) const;

	/** @return Name displayed for other users */
	const std::string& GetDisplayedName() const;

	/** @return User status depending on last time active */
	EUserStatus GetUserStatus() const;

protected:
	Uint64 GetCurrentTime() const;

private:
	/** Displayed name to other users */
	std::string DisplayedName;

	/** Username (for log in) */
	std::string UserName;

	/** User password (for log in) */
	std::string UserPassword;

	/** User E-Mail for password recovery (@TODO in future) */
	std::string UserEMail;

	/** Timestamp of last activity */
	Uint64 LastActiveTime;

	/** User manager class */
	FUserManager* UserManager;
	
};
