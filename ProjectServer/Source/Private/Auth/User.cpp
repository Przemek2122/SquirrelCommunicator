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

void FUser::SetPassword(const std::string& InUserPassword)
{
	const std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	UserPassword = Encryptor->HashPasswordCustom(InUserPassword, GetArgonSettings());
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

Uint64 FUser::GetUserId() const
{
	return UserId;
}

Uint64 FUser::GetCurrentTime() const
{
	return UserManager->GetCurrentTimeCached();
}

FArgonSettings FUser::GetArgonSettings() const
{
	return FArgonSettings(2, 15 * 1024, 1, 128, 64);
}
