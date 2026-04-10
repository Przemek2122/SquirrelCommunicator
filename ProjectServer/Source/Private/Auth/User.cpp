#include "Auth/UserManager.h"

std::string UserStatusToString(EUserStatus UserStatus)
{
	std::string Out;

	switch (UserStatus)
	{
		case EUserStatus::Unknown:
		{
			Out = "unknown";

			break;
		}
		case EUserStatus::Online:
		{
			Out = "online";

			break;
		}
		case EUserStatus::Offline:
		{
			Out = "offline";

			break;
		}
	}

	return Out;
}

FUser::FUser(FUserManager* InUserManager)
	: UserManager(InUserManager)
	, SocketId(-1)
	, UserStatus(EUserStatus::Offline)
{
}

void FUser::UpdateLastActiveTime()
{
	LastActiveTime = GetCurrentTime();
}

void FUser::SetUserName(const std::string& InUserName)
{
	std::unique_lock Lock(UserNameMutex);
	UserName = InUserName;
}

void FUser::SetUserEMail(const std::string& InUserEMail)
{
	std::unique_lock Lock(UserEMailMutex);
	UserEMail = InUserEMail;
}

void FUser::SetUserId(const Uint64 InUserId)
{
	UserId = InUserId;
}

void FUser::SetPassword(const std::string& InUserEncryptedPassword)
{
	std::unique_lock Lock(UserPasswordHashMutex);
	UserPasswordHash = InUserEncryptedPassword;
}

bool FUser::IsUserNameCorrect(const std::string& InUserName) const
{
	return (UserName == InUserName);
}

bool FUser::IsUserMailCorrect(const std::string& InMailName) const
{
	return (UserEMail == InMailName);
}

bool FUser::IsUserPasswordCorrect(const std::string& InUserPasswordHash) const
{
	return (UserPasswordHash == InUserPasswordHash);
}

void FUser::SetSocketId(int32 InSocketId)
{
	SocketId = InSocketId;
}

void FUser::SetUserStatus(EUserStatus NewUserStatus)
{
	UserStatus = NewUserStatus;
}

std::string FUser::GetUserNameString()
{
	std::shared_lock Lock(UserNameMutex);
	return UserName;
}

std::string FUser::GetUserPasswordHash()
{
	std::shared_lock Lock(UserPasswordHashMutex);
	return UserPasswordHash;
}

std::string FUser::GetUserMail()
{
	std::shared_lock Lock(UserEMailMutex);
	return UserEMail;
}

EUserStatus FUser::GetUserStatus() const
{
	return UserStatus;
}

Uint64 FUser::GetUserId() const
{
	return UserId;
}

int32 FUser::GetSocketId() const
{
	return SocketId;
}

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}
