#include "Auth/UserManager.h"

#include <nlohmann/json.hpp>

#include "DataBase/DataBaseConnect.h"
#include "Misc/EncryptionManager.h"
#include "Misc/EncryptionUtil.h"

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

	// Check password
	if (!ValidatePasswordLength(InUserPassword))
	{
		RegisterUserStatus = ERegisterUserStatus::PasswordLengthIncorrect;
	}

	// Check mail
	if (!ValidateEMailLength(InUserEMail))
	{
		RegisterUserStatus = ERegisterUserStatus::MailLengthIncorrect;
	}

	// Check User name
	if (!ValidateUserNameLength(InUserName))
	{
		RegisterUserStatus = ERegisterUserStatus::UserNameLengthIncorrect;
	}

	// Check mail
	if (!FStringHelpers::ValidateMail(InUserEMail))
	{
		RegisterUserStatus = ERegisterUserStatus::MailIncorrect;
	}

	if (!FStringHelpers::ValidateString(InUserPassword, FPredefinedCharsets::BASE_SIMPLE_PASSWORD))
	{
		RegisterUserStatus = ERegisterUserStatus::PasswordIncorrect;
	}
	
	if (RegisterUserStatus == ERegisterUserStatus::Unknown)
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

ERegisterUserStatus FUserManager::RegisterIntegration(const std::string& InUserName, const std::string& InUserEMail)
{
	ERegisterUserStatus RegisterUserStatus = ERegisterUserStatus::Unknown;

	// Check mail
	if (!ValidateEMailLength(InUserEMail))
	{
		RegisterUserStatus = ERegisterUserStatus::MailLengthIncorrect;
	}

	// Check User name
	if (!ValidateUserNameLength(InUserName))
	{
		RegisterUserStatus = ERegisterUserStatus::UserNameLengthIncorrect;
	}

	if (RegisterUserStatus == ERegisterUserStatus::Unknown)
	{
		
	}

	return RegisterUserStatus;
}

ELoginStatus FUserManager::LoginUser(const std::string& InUserEmail, const std::string& InUserPassword, std::string& OutSessionToken)
{
	ELoginStatus LoginStatus = ELoginStatus::IncorrectCredentialsOrUserDoesNotExist;

	// Basic email check
	if (!ValidateEMailLength(InUserEmail))
	{
		return ELoginStatus::IncorrectInputLength;
	}

	// Basic password check
	if (!ValidatePasswordLength(InUserPassword))
	{
		return ELoginStatus::IncorrectInputLength;
	}

	std::shared_ptr<FUser> UserPtr = nullptr;
	bool bWereDownloadedFromDB = false;

	{
		const std::shared_lock ScopeLock(UserDataBaseMutex);
		for (const std::pair<const Uint64, std::shared_ptr<FUser>>& UserPair : UserDataBaseCache)
		{
			if (UserPair.second->IsUserMailCorrect(InUserEmail))
			{
				UserPtr = UserPair.second;
			}
		}
	}

	// User missing check db
	if (UserPtr == nullptr)
	{
		EDatabaseOperationResult Result = DownloadUserFromDBByMail(InUserEmail, UserPtr);
		if (Result == EDatabaseOperationResult::Success)
		{
			bWereDownloadedFromDB = true;
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

ELoginStatus FUserManager::LoginIntegration(const std::string& InUserEmail, std::string& OutSessionToken)
{
	ELoginStatus LoginStatus = ELoginStatus::IncorrectCredentialsOrUserDoesNotExist;

	// Basic email check
	if (!ValidateEMailLength(InUserEmail))
	{
		return ELoginStatus::IncorrectInputLength;
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

	const std::shared_lock ScopeLock(UserDataBaseMutex);
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

void FUserManager::UpdateUserActivity(const Uint64 UsedId)
{
	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		// Get database connection session
		soci::session& DataBaseSession = Connect.GetSession();

		// Update activity time
		DataBaseSession << "UPDATE users SET LastActive = NOW() WHERE id = :id",
			soci::use(UsedId, "id");

		// Update cache
		std::vector<std::shared_ptr<FUser>> Users;
		const bool bGetUsers = GetUsersByIds({ UsedId }, Users);
		if (bGetUsers)
		{
			std::shared_ptr<FUser>& FirstUser = Users[0];
			FirstUser->UpdateLastActiveTime();
		}
	}
}

std::shared_ptr<FUser> FUserManager::FindUserByMail(const std::string& InMail)
{
	const std::shared_lock<std::shared_mutex> UserMailMapMutexScopeLock(UserMailMapMutex);

	std::shared_ptr<FUser> Out;

	if (MailToUserIdMap.ContainsKey(InMail))
	{
		const std::shared_lock<std::shared_mutex> UserDataBaseCacheScopeLock(UserDataBaseMutex);

		Out = UserDataBaseCache[MailToUserIdMap[InMail]];
	}

	return Out;
}

Uint64 FUserManager::GetIdFromToken(const std::string& InToken) const
{
	const Uint64 Id = SessionManager->GetUserIdFromSessionId(InToken);
	return Id;
}

bool FUserManager::GetUsersByIds(const std::vector<Uint64>& UserIds, std::vector<std::shared_ptr<FUser>>& OutUsers)
{
	return (DownloadUsersFromDBByIds(UserIds, OutUsers, true) == EDatabaseOperationResult::Success);
}

EDatabaseOperationResult FUserManager::DownloadUserFromDBByMail(const std::string& InUserEmail, std::shared_ptr<FUser>& UserPtr)
{
	EDatabaseOperationResult DownloadResult = EDatabaseOperationResult::Unknown;

	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		// Get database connection session
		soci::session& DataBaseSession = Connect.GetSession();

		try
		{
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

				DownloadResult = EDatabaseOperationResult::Success;
			}
			else
			{
				DownloadResult = EDatabaseOperationResult::DataNotFound;
			}
		}
		catch (const nlohmann::json::exception& e)
		{
			LOG_ERROR("Database error: " << e.what());

			DownloadResult = EDatabaseOperationResult::DatabaseFailed;
		}
	}
	else
	{
		DownloadResult = EDatabaseOperationResult::ConnectionFailed;
	}

	return DownloadResult;
}

EDatabaseOperationResult FUserManager::DownloadUsersFromDBByIds(const std::vector<Uint64>& UserIds, std::vector<std::shared_ptr<FUser>>& OutUsers, const bool bAutoAddToCache)
{
	if (UserIds.empty())
	{
		return EDatabaseOperationResult::DataNotFound;
	}

	// 1. Prepare containers
	OutUsers.clear();
	OutUsers.reserve(UserIds.size());

	std::vector<Uint64> MissingIds;
	MissingIds.reserve(UserIds.size());

	// 2. CACHE PASS: Check what we already have
	for (Uint64 TargetId : UserIds)
	{
		const std::shared_lock ScopeLock(UserDataBaseMutex);
		std::optional<std::shared_ptr<FUser>> ExistingUserCache = UserDataBaseCache.FindValueByKey(TargetId);

		if (ExistingUserCache.has_value())
		{
			OutUsers.push_back(ExistingUserCache.value());
		}
		else
		{
			MissingIds.push_back(TargetId);
		}
	}

	// If we found everyone in the cache, we are done!
	if (MissingIds.empty())
	{
		return EDatabaseOperationResult::Success;
	}

	// 3. DATABASE PASS: Download only missing IDs
	EDatabaseOperationResult DownloadResult = EDatabaseOperationResult::Unknown;
	FDataBaseConnect Connect;

	if (Connect.IsConnected())
	{
		soci::session& DataBaseSession = Connect.GetSession();

		try
		{
			// Build string for SQL "IN" clause: "1, 5, 99"
			std::string IdList;
			for (size_t i = 0; i < MissingIds.size(); ++i) {
				IdList += std::to_string(MissingIds[i]);
				if (i < MissingIds.size() - 1) IdList += ",";
			}

			// Prepare fetch variables
			std::string Username, StoredPasswordHash, Mail, DisplayName;
			Uint64 UserIdVal;
			soci::indicator Ind;

			// Execute Single Query
			soci::statement St = (DataBaseSession.prepare <<
				"SELECT id, username, password, email, displayedname FROM users WHERE id IN (" + IdList + ")",
				soci::into(UserIdVal, Ind),
				soci::into(Username),
				soci::into(StoredPasswordHash),
				soci::into(Mail),
				soci::into(DisplayName)
				);

			St.execute();

			while (St.fetch())
			{
				if (Ind == soci::i_ok)
				{
					// Create User
					std::shared_ptr<FUser> NewUser = std::make_shared<FUser>(this);
					NewUser->SetUserName(Username);
					NewUser->SetPassword(StoredPasswordHash);
					NewUser->SetUserEMail(Mail);
					NewUser->SetDisplayedName(DisplayName);
					NewUser->SetUserId(UserIdVal);

					// Update CACHE
					if (bAutoAddToCache)
					{
						const std::unique_lock ScopeLock(UserDataBaseMutex);
						UserDataBaseCache.Emplace(UserIdVal, NewUser);
					}

					// Add to Output
					OutUsers.push_back(NewUser);
				}
			}

			// If we have any users (from cache OR db), consider it a success
			DownloadResult = (OutUsers.empty()) ? EDatabaseOperationResult::DataNotFound : EDatabaseOperationResult::Success;
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Database error (Bulk User Resolve): " << e.what());
			DownloadResult = EDatabaseOperationResult::DatabaseFailed;
		}
	}
	else
	{
		DownloadResult = EDatabaseOperationResult::ConnectionFailed;
	}

	return DownloadResult;
}

Uint64 FUserManager::GenerateNextAvailableId()
{
	NextAvailableIndex++;

	const std::shared_lock ScopeLock(UserDataBaseMutex);
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

		UpdateUserActivity(UserPtr->GetUserId());
	}
}

void FUserManager::OnRegisterSuccessful(const std::shared_ptr<FUser>& UserPtr)
{
	AddUserToCache(UserPtr);
}

void FUserManager::AddUserToCache(const std::shared_ptr<FUser>& UserPtr)
{
	// Lock as register may come from any thread
	const std::unique_lock UserDataBaseMutexScopeLock(UserDataBaseMutex);

	// Create user
	UserDataBaseCache.Emplace(UserPtr->GetUserId(), UserPtr);

	// Another lock for map
	const std::unique_lock UserMailMapMutexScopeLock(UserMailMapMutex);

	// Add mail to cache
	MailToUserIdMap.Emplace(UserPtr->GetUserMail(), UserPtr->GetUserId());
}

bool FUserManager::ValidateUserNameLength(const std::string& InUserName)
{
	return (InUserName.size() > 4 && InUserName.size() < 110);
}

bool FUserManager::ValidatePasswordLength(const std::string& InPassword)
{
	return (InPassword.length() > 7) && (InPassword.size() < 270);
}

bool FUserManager::ValidateEMailLength(const std::string& InEMail)
{
	return InEMail.length() > 4 && (InEMail.size() < 530);
}
