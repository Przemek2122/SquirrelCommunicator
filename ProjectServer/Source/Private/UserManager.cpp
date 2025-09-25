#include "UserManager.h"

FUser::FUser(FUserManager* InUserManager)
	: LastActiveTime(0)
	, UserManager(InUserManager)
{
}

void FUser::UpdateLastActiveTime()
{
	LastActiveTime = GetCurrentTime();
}

bool FUser::IsUserNameCorrect(const std::string& InUserName) const
{
	return (UserName == InUserName);
}

bool FUser::IsUserPasswordCorrect(const std::string& InUserPassword) const
{
	// @TODO Encryption will be needed here
	return (UserPassword == InUserPassword);
}

const std::string& FUser::GetDisplayedName() const
{
	return DisplayedName;
}

FUserStatus FUser::GetUserStatus() const
{
	static constexpr Uint64 TimeWhileActive = 180;

	return ( ((LastActiveTime + TimeWhileActive) > GetCurrentTime()) ? FUserStatus::Online : FUserStatus::Offline );
}

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}

void FUserManager::PostSecondTick()
{
	CurrentTimeCached = FUtil::GetRawTime();
}

bool FUserManager::DoesUserExist(const std::string& InUserName)
{
	bool bDoesUserExist = false;

	for (const FUser& User : UserDataBase)
	{
		if (User.IsUserNameCorrect(InUserName))
		{
			bDoesUserExist = true;

			break;
		}
	}

	return bDoesUserExist;
}

bool FUserManager::AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword)
{
	bool bAreLoginCredentialsCorrect = false;

	for (const FUser& User : UserDataBase)
	{
		if (User.IsUserNameCorrect(InUserName) && User.IsUserPasswordCorrect(InUserPassword))
		{
			bAreLoginCredentialsCorrect = true;

			break;
		}
	}

	return bAreLoginCredentialsCorrect;
}
