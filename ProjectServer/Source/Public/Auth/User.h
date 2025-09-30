#pragma once

#include "CoreMinimal.h"
#include "Misc/PasswordEncryptionArgon.h"

class FUserManager;

enum class EUserStatus : Uint8
{
	Unknown,
	Online,
	Offline
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
	void SetUserId(const Uint64 InUserId);

	bool IsUserNameCorrect(const std::string& InUserName) const;
	bool IsUserPasswordCorrect(const std::string& InUserPassword) const;

	/** @return Name displayed for other users */
	const std::string& GetDisplayedName() const;

	/** @return User status depending on last time active */
	EUserStatus GetUserStatus() const;

	/** Get unique user id */
	Uint64 GetUserId() const;

protected:
	Uint64 GetCurrentTime() const;

	FArgonSettings GetArgonSettings() const;

private:
	/** Displayed name to other users */
	std::string DisplayedName;

	/** Username (for log in) */
	std::string UserName;

	/** User password (for log in) */
	std::string UserPassword;

	/** User E-Mail for password recovery (@TODO in future) */
	std::string UserEMail;

	/** Unique user Id */
	Uint64 UserId;

	/** Timestamp of last activity */
	Uint64 LastActiveTime;

	/** User manager class */
	FUserManager* UserManager;
	
};
