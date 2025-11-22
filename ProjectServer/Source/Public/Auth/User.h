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

/** Class only with user data */
class FUserData
{
public:
	FUserData()
		: UserId(0)
		, LastActiveTime(0)
	{
	}

	bool IsValid() const { return (UserId > 0); }

protected:
	/** Displayed name to other users */
	std::string DisplayedName;

	/** Username (for log in) */
	std::string UserName;

	/** User password (for log in) */
	std::string UserPasswordHash;

	/** User E-Mail for password recovery (@TODO in future) */
	std::string UserEMail;

	/** Unique user Id */
	Uint64 UserId;

	/** Timestamp of last activity */
	Uint64 LastActiveTime;
};

/** Class for single user */
class FUser : public FUserData
{
public:
	FUser(FUserManager* InUserManager = nullptr);

	/** Update last active time with current time */
	void UpdateLastActiveTime();

	void SetDisplayedName(const std::string& InDisplayedName);
	void SetUserName(const std::string& InUserName);
	void SetPassword(const std::string& InUserEncryptedPassword);
	void SetUserEMail(const std::string& InUserEMail);
	void SetUserId(const Uint64 InUserId);

	bool IsUserNameCorrect(const std::string& InUserName) const;
	bool IsUserMailCorrect(const std::string& InMailName) const;
	bool IsUserPasswordCorrect(const std::string& InUserPasswordHash) const;

	/** @return Name displayed for other users */
	const std::string& GetDisplayedName() const;

	const std::string& GetUserPasswordHash() const;

	/** @return User status depending on last time active */
	EUserStatus GetUserStatus() const;

	/** Get unique user id */
	Uint64 GetUserId() const;

	FUserData GetUserData() const;

protected:
	Uint64 GetCurrentTime() const;

private:
	/** User manager class */
	FUserManager* UserManager;
	
};
