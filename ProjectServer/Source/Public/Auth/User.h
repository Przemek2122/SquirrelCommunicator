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

std::string UserStatusToString(EUserStatus UserStatus);

/** Class for single user */
class FUser : public FUserData
{
public:
	FUser(FUserManager* InUserManager = nullptr);

	/** Update last active time with current time */
	void UpdateLastActiveTime();

	void SetUserName(const std::string& InUserName);
	void SetPassword(const std::string& InUserEncryptedPassword);
	void SetUserEMail(const std::string& InUserEMail);
	void SetUserId(const Uint64 InUserId);

	bool IsUserNameCorrect(const std::string& InUserName) const;
	bool IsUserMailCorrect(const std::string& InMailName) const;
	bool IsUserPasswordCorrect(const std::string& InUserPasswordHash) const;

	void SetSocketId(int32 InSocketId);
	void SetUserStatus(EUserStatus NewUserStatus);

	/** Get username */
	const std::string& GetUserNameString() const;

	/** Get user password in hash form */
	const std::string& GetUserPasswordHash() const;

	/** Get user mail */
	const std::string& GetUserMail() const { return UserEMail; }

	/** @return User status depending on last time active */
	EUserStatus GetUserStatus() const;

	/** Get unique user id */
	Uint64 GetUserId() const;
	FUserData GetUserData() const;
	int32 GetSocketId() const;

protected:
	Uint64 GetCurrentTime() const;

private:
	/** User manager class */
	FUserManager* UserManager;

	/** Socket Id pointer */
	int32 SocketId;

	EUserStatus UserStatus;

	/**  */
	//bool bIsConnected;
	
};
