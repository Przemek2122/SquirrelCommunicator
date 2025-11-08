#include "Auth/UserManager.h"
#include "Misc/EncryptionManager.h"
#include "Misc/PasswordEncryptionArgon.h"

FUser::FUser(FUserManager* InUserManager)
	: UserId(0)
	, LastActiveTime(0)
	, UserManager(InUserManager)
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
	static constexpr Uint64 TimeWhileActive = 180;

	return ( ((LastActiveTime + TimeWhileActive) > GetCurrentTime()) ? EUserStatus::Online : EUserStatus::Offline );
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

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}
