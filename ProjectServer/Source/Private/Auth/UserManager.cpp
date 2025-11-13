#include "Auth/UserManager.h"

#include "DataBase/DataBaseConnect.h"
#include "Misc/EncryptionManager.h"

FUserManager::FUserManager()
	: SessionManager(new FSessionManager())
	, NextAvailableIndex(0)
	, CurrentTimeCached(0)
{
}

FUserManager::~FUserManager()
{
}

void FUserManager::Init()
{
	CachedArgonSettings = GetArgonSettings();

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

	bool bCanRegister = false;

	// Verifications
	if (InUserPassword.length() > 8)
	{
		bCanRegister = true;
	}
	else
	{
		RegisterUserStatus = ERegisterUserStatus::PasswordToWeak;
	}

	if (bCanRegister)
	{
		const std::shared_ptr<FUser> UserPtr = std::make_shared<FUser>(this);
		FUser* User = UserPtr.get();
		User->SetDisplayedName(InUserName);
		User->SetUserName(InUserName);
		User->SetPassword(HashUserPassword(InUserPassword));
		User->SetUserEMail(InUserEMail);
		User->UpdateLastActiveTime();

		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();

			// Check if mail is taken // sql << "select name from person where id = 7", into(name, ind);
			std::string Username;
			soci::indicator Ind;

			DataBaseSession << "SELECT username FROM users WHERE email = :email",
				soci::use(InUserEMail),
				soci::into(Username, Ind);

			if (Ind != soci::i_ok)
			{
				// Create user
				DataBaseSession.once << "INSERT INTO users(username, password, email, displayedname) VALUES(:un, :ps, :em, :dun)",
					soci::use(InUserName, "un"),
					soci::use(User->GetUserPasswordHash(), "ps"),
					soci::use(InUserEMail, "em"),
					soci::use(InUserName, "dun");

				// Get id
				Uint64 Id = 0;
				DataBaseSession.once << "SELECT LAST_INSERT_ID()",
					soci::into(Id);

				if (Id > 0)
				{
					User->SetUserId(Id);

					RegisterUserStatus = ERegisterUserStatus::Successful;

					OnRegisterSuccessful(UserPtr);
				}
				else
				{
					RegisterUserStatus = ERegisterUserStatus::DataBaseInsertFailed;
				}
			}
			else
			{
				RegisterUserStatus = ERegisterUserStatus::MailTaken;
			}
		}
		else
		{
			RegisterUserStatus = ERegisterUserStatus::DataBaseConnectionFailed;
		}
	}

	return RegisterUserStatus;
}

ELoginStatus FUserManager::LoginUser(const std::string& InUserEmail, const std::string& InUserPassword, std::string& OutSessionToken)
{
	ELoginStatus LoginStatus = ELoginStatus::IncorrectCredentialsOrUserDoesNotExist;

	std::shared_ptr<FUser> UserPtr = nullptr;
	bool bWereDownloadedFromDB = false;

	for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBaseCache)
	{
		if (UserPair.second->IsUserMailCorrect(InUserEmail))
		{
			UserPtr = UserPair.second;
		}
	}

	// User missing check db
	if (UserPtr == nullptr)
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();

			// Get user data and hashed password
			std::string Username;
			std::string StoredPasswordHash;
			std::string Mail;
			std::string DisplayName;
			Uint64 UserId;
			soci::indicator Ind;

			DataBaseSession << "SELECT id, username, password, email, displayedname FROM users WHERE email = :email",
				soci::use(InUserEmail),
				soci::into(UserId, Ind),
				soci::into(Username),
				soci::into(StoredPasswordHash),
				soci::into(Mail),
				soci::into(DisplayName);

			if (Ind == soci::i_ok)
			{
				UserPtr = std::make_shared<FUser>(this);
				FUser* User = UserPtr.get();
				User->SetUserName(Username);
				User->SetPassword(StoredPasswordHash);
				User->SetUserEMail(Mail);
				User->SetDisplayedName(DisplayName);
				User->SetUserId(UserId);

				bWereDownloadedFromDB = true;
			}
		}
		else
		{
			LoginStatus = ELoginStatus::DataBaseConnectionFailed;
		}
	}

	if (UserPtr != nullptr)
	{
		// @TODO: Should we support BAN?

		if (VerifyPasswords(UserPtr->GetUserPasswordHash(), InUserPassword))
		{
			FDataBaseConnect Connect;
			if (Connect.IsConnected())
			{
				// Get database connection session
				soci::session& DataBaseSession = Connect.GetSession();

				const Uint64 Id = UserPtr->GetUserId();

				// Update activity time
				DataBaseSession << "UPDATE users SET LastActive = NOW() WHERE id = :id",
					soci::use(Id, "id");

				OnLoginSuccessful(UserPtr, bWereDownloadedFromDB);

				OutSessionToken = SessionManager->CreateSession(Id);

				LoginStatus = ELoginStatus::Successful;
			}
			else
			{
				LoginStatus = ELoginStatus::DataBaseConnectionFailed;
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

bool FUserManager::AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword)
{
	bool bAreLoginCredentialsCorrect = false;

	for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBaseCache)
	{
		FUser* User = UserPair.second.get();
		if (User->IsUserPasswordCorrect(InUserPassword))
		{
			bAreLoginCredentialsCorrect = true;
		}
	}

	return bAreLoginCredentialsCorrect;
}

bool FUserManager::VerifyToken(const std::string& InToken) const
{
	const Uint64 Id = SessionManager->GetUserIdFromSessionId(InToken);
	return (Id > 0);
}

bool FUserManager::RefreshSessionToken(const std::string& InToken) const
{
	return SessionManager->IsSessionTokenAlive(InToken);
}

Uint64 FUserManager::GetIdFromToken(const std::string& InToken) const
{
	const Uint64 Id = SessionManager->GetUserIdFromSessionId(InToken);
	return Id;
}

Uint64 FUserManager::GenerateNextAvailableId()
{
	NextAvailableIndex++;

	if (UserDataBaseCache.ContainsKey(NextAvailableIndex))
	{
		LOG_ERROR("Critical error, NextAvailableIndex already exist and should not!");

		// Find first available index
		while (UserDataBaseCache.ContainsKey(NextAvailableIndex))
		{
			NextAvailableIndex++;
		}
	}

	return NextAvailableIndex;
}

bool FUserManager::VerifyPasswords(const std::string& StringWithHash, const std::string& StringWithoutHash)
{
	const std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	return Encryptor->VerifyPassword(StringWithHash, StringWithoutHash);
}

std::string FUserManager::HashUserPassword(const std::string& RawPassword)
{
	const std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	return Encryptor->HashPasswordCustom(RawPassword, GetArgonSettings());
}

FArgonSettings FUserManager::GetArgonSettings() const
{
	return FArgonSettings(2, 15 * 1024, 1, 128, 64);
}

void FUserManager::OnLoginSuccessful(const std::shared_ptr<FUser>& UserPtr, const bool bWereDownloadedFromDB)
{
	if (bWereDownloadedFromDB)
	{
		AddUserToCache(UserPtr);
	}
}

void FUserManager::OnRegisterSuccessful(const std::shared_ptr<FUser>& UserPtr)
{
	AddUserToCache(UserPtr);
}

void FUserManager::AddUserToCache(const std::shared_ptr<FUser>& UserPtr)
{
	// Lock as register may come from any thread
	const std::lock_guard<std::mutex> ThreadScopeLock(UserDataBaseMutex);

	// Create user
	UserDataBaseCache.Emplace(UserPtr->GetUserId(), UserPtr);
}
