#include "Auth/UserManager.h"

#include "Misc/Serializer.h"
#include "Types/Mutex/MutexScopeLock.h"

FUserManager::FUserManager()
	: SessionManager(new FSessionManager())
	, NextAvailableIndex(0)
	, CurrentTimeCached(0)
{
	FAssetsManager* AssetsManager = FGlobalDefines::GEngine->GetAssetsManager();

	const std::string UserDataBaseRelativePath = AssetsManager->GetConfigPathRelative() + "UserDataBase.fser";
	UserDataBaseFilePath = AssetsManager->ConvertRelativeToFullPath(UserDataBaseRelativePath);
	UserDataBaseBackupFilePath = AssetsManager->ConvertRelativeToFullPath(UserDataBaseRelativePath + ".backup");

	LoadUsers();
}

FUserManager::~FUserManager()
{
	SaveUsersWithBackup();
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
	if (FFileSystem::File::Exists(UserDataBaseFilePath))
	{
		// Lock database - nothing exists so we just wait until loaded
		FMutexScopeLock UserDataBaseMutexScopeLock(UserDataBaseMutex);

		CUnorderedMap<Uint64, std::shared_ptr<FUser>, Uint64> UserDataBaseCopy;
		Uint64 NextAvailableIndexCopy = 0;

		size_t Offset = 0;
		std::vector<char> SerializeData;
		FSerializer::Load(UserDataBaseFilePath, SerializeData);

		DESERIALIZE_FIELD(SerializeData, Offset, NextAvailableIndexCopy);
		DESERIALIZE_FIELD(SerializeData, Offset, UserDataBaseCopy.Map);

		NextAvailableIndex = std::move(NextAvailableIndexCopy);
		UserDataBase.Map = UserDataBaseCopy.Map;

		LOG_INFO("Users loaded");
	}
}

void FUserManager::SaveUsers()
{
	CUnorderedMap<Uint64, std::shared_ptr<FUser>, Uint64> UserDataBaseCopy;
	Uint64 NextAvailableIndexCopy = 0;

	{
		// Lock database
		FMutexScopeLock UserDataBaseMutexScopeLock(UserDataBaseMutex);

		// Perform actuall copy
		NextAvailableIndexCopy = NextAvailableIndex;
		UserDataBaseCopy.Map = UserDataBase.Map;
	}

	std::vector<char> SerializeData;

	// Serialize index
	SERIALIZE_FIELD(SerializeData, NextAvailableIndexCopy);

	// Serialize users
	SERIALIZE_FIELD(SerializeData, UserDataBaseCopy.Map);

	FSerializer::Save(UserDataBaseFilePath, SerializeData);

	LOG_INFO("Users saved");
}

void FUserManager::SaveUsersWithBackup()
{
	// Make backup if exists
	if (FFileSystem::File::Exists(UserDataBaseFilePath))
	{
		// Delete previous copy if exists
		if (FFileSystem::File::Exists(UserDataBaseBackupFilePath))
		{
			FFileSystem::File::Delete(UserDataBaseBackupFilePath);
		}

		FFileSystem::File::Rename(UserDataBaseFilePath, UserDataBaseBackupFilePath);
	}

	SaveUsers();
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
