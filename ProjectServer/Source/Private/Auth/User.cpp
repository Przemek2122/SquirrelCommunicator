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

void FUser::SetDisplayedName(const std::string& InDisplayedName)
{
	DisplayedName = InDisplayedName;
}

void FUser::SetUserName(const std::string& InUserName)
{
	UserName = InUserName;
}

void FUser::SetUserEMail(const std::string& InUserEMail)
{
	UserEMail = InUserEMail;
}

void FUser::SetUserId(const Uint64 InUserId)
{
	UserId = InUserId;
}

void FUser::SetPassword(const std::string& InUserEncryptedPassword)
{
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

const std::string& FUser::GetDisplayedName() const
{
	return DisplayedName;
}

const std::string& FUser::GetUserPasswordHash() const
{
	return UserPasswordHash;
}

EUserStatus FUser::GetUserStatus() const
{
	return UserStatus;
}

Uint64 FUser::GetUserId() const
{
	return UserId;
}

FUserData FUser::GetUserData() const
{
	FUserData SavableUserData = *this;

	return SavableUserData;
}

int32 FUser::GetSocketId() const
{
	return SocketId;
}

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}
