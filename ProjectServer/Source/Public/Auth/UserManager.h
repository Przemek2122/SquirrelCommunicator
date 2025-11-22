#pragma once

#include "CoreMinimal.h"
#include "SessionManager.h"
#include "User.h"

enum class ERegisterUserStatus : Uint8
{
	Unknown,
	Successful,
	MailTaken,
	PasswordToWeak,
	DataBaseInsertFailed,
	DataBaseConnectionFailed
};

enum class ELoginStatus : Uint8
{
	Unknown,
	Successful,
	SessionAlreadyExist,
	IncorrectCredentialsOrUserDoesNotExist,
	DataBaseFetchFailed,
	DataBaseConnectionFailed
};

/** Class for managing users */
class FUserManager
{
public:
	FUserManager();
	~FUserManager();

	void Init();

	/** Updated every second */
	void PostSecondTick();

	/**
	 * Use to register
	 * @return registration status, see enum for details
	 */
	ERegisterUserStatus RegisterUser(const std::string& InUserName, const std::string& InUserPassword, const std::string& InUserEMail);

	/**
	 * Use for login
	 * @return Unique session token
	 */
	ELoginStatus LoginUser(const std::string& InUserEmail, const std::string& InUserPassword, std::string& OutSessionToken);

	/** @return true if successfully logged out */
	bool Logout(const std::string& InSessionToken);

	bool AreLoginCredentialsCorrect(const std::string& InUserName, const std::string& InUserPassword);

	/** @return true when valid. Check if provided token is correct. */
	bool VerifyToken(const std::string& InToken) const;

	/** @return true when valid. Will leave old token but with longer time to use. */
	bool RefreshSessionToken(const std::string& InToken) const;

	/** Get user ID from token, 0 means not found */
	Uint64 GetIdFromToken(const std::string& InToken) const;

	/** Cache for time updated every second */
	Uint64 GetCurrentTimeCached() const { return CurrentTimeCached; }

	/** Search provided Id for User */
	FUser* GetUser(Uint64 UserId) const;

protected:
	Uint64 GenerateNextAvailableId();
	bool VerifyPasswords(const std::string& StringWithHash, const std::string& StringWithoutHash);
	std::string HashUserPassword(const std::string& RawPassword);
	FArgonSettings GetArgonSettings() const;

	void OnLoginSuccessful(const std::shared_ptr<FUser>& UserPtr, const bool bWereDownloadedFromDB);
	void OnRegisterSuccessful(const std::shared_ptr<FUser>& UserPtr);
	void AddUserToCache(const std::shared_ptr<FUser>& UserPtr);

private:
	/** Manager for user sessions */
	std::unique_ptr<FSessionManager> SessionManager;

	/**
	 * Map with all users connected to number,
	 * cached version of users in database, on restart it will be rebuilt from database
	 * @TODO For long runs consider clearing every x time of inactivity
	 */
	CUnorderedMap<Uint64, std::shared_ptr<FUser>, Uint64> UserDataBaseCache;

	/** Tells us which number is available */
	Uint64 NextAvailableIndex;

	/** Mutex for UserDataBase */
	std::mutex UserDataBaseMutex;

	/** Cache for time, we will use it for each login, message, etc so cache will be faster */
	Uint64 CurrentTimeCached;

	FArgonSettings CachedArgonSettings;

};
