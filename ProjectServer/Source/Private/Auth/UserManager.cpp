#include "Auth/UserManager.h"
#include "Types/Mutex/MutexScopeLock.h"

FUserManager::FUserManager()
	: SessionManager(new FSessionManager())
	, NextAvailableIndex(0)
	, CurrentTimeCached(0)
{
}

void FUserManager::Init()
{
	SessionManager->Init();
}

void FUserManager::PostSecondTick()
{
	CurrentTimeCached = FUtil::GetSeconds();

	SessionManager->PostSecondTick();
}

ERegisterUserStatus FUserManager::RegisterUser(const std::string& InUserName, const std::string& InUserPassword, const std::string& InUserEMail)
{
	ERegisterUserStatus RegisterUserStatus = ERegisterUserStatus::Unknown;

	if (!DoesUserExist(InUserName))
	{
		const Uint64 Id = GenerateNextAvailableId();

		const std::shared_ptr<FUser> UserPtr = std::make_shared<FUser>(this);
		FUser* User = UserPtr.get();
		User->SetDisplayedName(InUserName);
		User->SetUserName(InUserName);
		User->SetPassword(InUserPassword);
		User->SetUserEMail(InUserEMail);
		User->SetUserId(Id);
		User->UpdateLastActiveTime();

		RegisterUserStatus = ERegisterUserStatus::Successful;

		// Lock as register may come from any thread
		const FMutexScopeLock ThreadScopeLock(UserDataBaseMutex);

		// Create user
		UserDataBase.Emplace(Id, UserPtr);
	}

	return RegisterUserStatus;
}

ELoginStatus FUserManager::LoginUser(const std::string& InUserName, const std::string& InUserPassword, std::string& OutSessionToken)
{
	ELoginStatus LoginStatus = ELoginStatus::IncorrectCredentialsOrUserDoesNotExist;

	for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBase)
	{
		if (UserPair.second->IsUserNameCorrect(InUserName))
		{
			const Uint64 Id = UserPair.second->GetUserId();

			if (!SessionManager->DoesUserHaveSession(Id))
			{
				OutSessionToken = SessionManager->CreateSession(Id);

				// We should never get an empty session
				ENSURE_VALID(!OutSessionToken.empty());

				LoginStatus = ELoginStatus::Successful;
			}
			else
			{
				LoginStatus = ELoginStatus::SessionAlreadyExist;
			}
		}
	}

	return LoginStatus;
}

bool FUserManager::Logout(const std::string& InSessionToken)
{
	bool bLogoutSuccessful;

	bLogoutSuccessful = SessionManager->DeactivateSession(InSessionToken);

	return bLogoutSuccessful;
}

bool FUserManager::DoesUserExist(const std::string& InUserName)
{
	bool bDoesUserExist = false;

	for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBase)
	{
		if (UserPair.second->IsUserNameCorrect(InUserName))
		{
			bDoesUserExist = true;
		}
	}

	return bDoesUserExist;
}

bool FUserManager::AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword)
{
	bool bAreLoginCredentialsCorrect = false;

	for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBase)
	{
		FUser* User = UserPair.second.get();
		if (User->IsUserPasswordCorrect(InUserPassword))
		{
			bAreLoginCredentialsCorrect = true;
		}
	}

	return bAreLoginCredentialsCorrect;
}

void FUserManager::LoadUsers()
{
	// Remember to load UserDataBase, NextAvailableIndex


}

void FUserManager::SaveUsers()
{
	// Remember to save UserDataBase, NextAvailableIndex


}

void FUserManager::SaveUsersBackup()
{
}

Uint64 FUserManager::GenerateNextAvailableId()
{
	NextAvailableIndex++;

	if (UserDataBase.ContainsKey(NextAvailableIndex))
	{
		LOG_ERROR("Critical error, NextAvailableIndex already exist and should not!");

		// Find first available index
		while (UserDataBase.ContainsKey(NextAvailableIndex))
		{
			NextAvailableIndex++;
		}
	}

	return NextAvailableIndex;
}
