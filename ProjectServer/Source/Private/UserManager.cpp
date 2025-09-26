#include "UserManager.h"
#include "Misc/EncryptionManager.h"
#include "Misc/PasswordEncryptionArgon.h"
#include "Types/Mutex/MutexScopeLock.h"

FUser::FUser(FUserManager* InUserManager)
	: LastActiveTime(0)
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

void FUser::SetPassword(const std::string& InUserPassword)
{
	const std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	UserPassword = Encryptor->HashPassword(InUserPassword);
}

bool FUser::IsUserNameCorrect(const std::string& InUserName) const
{
	return (UserName == InUserName);
}

bool FUser::IsUserPasswordCorrect(const std::string& InUserPassword) const
{
	const std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	const bool bIsUserPasswordCorrect = Encryptor->VerifyPassword(UserPassword, InUserPassword);

	return bIsUserPasswordCorrect;
}

const std::string& FUser::GetDisplayedName() const
{
	return DisplayedName;
}

EUserStatus FUser::GetUserStatus() const
{
	static constexpr Uint64 TimeWhileActive = 180;

	return ( ((LastActiveTime + TimeWhileActive) > GetCurrentTime()) ? EUserStatus::Online : EUserStatus::Offline );
}

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}

FUserManager::FUserManager()
	: CurrentTimeCached(0)
{
}

void FUserManager::PostSecondTick()
{
	CurrentTimeCached = FUtil::GetRawTime();
}

ERegisterUserStatus FUserManager::RegisterUser(const std::string& InUserName, const std::string& InUserPassword, const std::string& InUserEMail)
{
	ERegisterUserStatus RegisterUserStatus = ERegisterUserStatus::Unknown;

	if (!DoesUserExist(InUserName))
	{
		std::shared_ptr<FUser> UserPtr = std::make_shared<FUser>(this);
		FUser* User = UserPtr.get();
		User->SetDisplayedName(InUserName);
		User->SetUserName(InUserName);
		User->SetPassword(InUserPassword);
		User->SetUserEMail(InUserEMail);
		User->UpdateLastActiveTime();

		RegisterUserStatus = ERegisterUserStatus::Successful;

		// Lock as register may come from any thread
		FMutexScopeLock ThreadScopeLock(UserDataBaseMutex);

		// Create user
		UserDataBase.Push(UserPtr);
	}

	return RegisterUserStatus;
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

void FUserManager::LoadUsers()
{
}

void FUserManager::SaveUsers()
{
}

void FUserManager::SaveUsersBackup()
{
}
