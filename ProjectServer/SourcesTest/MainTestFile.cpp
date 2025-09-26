#include <gtest/gtest.h>
#include "Misc/EncryptionManager.h"
#include "Misc/PasswordEncryptionArgon.h"
//#include "Public/Project.h"

TEST(Test, EncryptionTest)
{
	const std::string CorrectString = "MyT4STStringu";
	const std::string IncorrectString = "STStringu";

	std::unique_ptr<FPasswordEncryptionArgon> Encryptor = FEncryptionManager::CreateEncryptorForPassword<FPasswordEncryptionArgon>();
	const std::string PassHash = Encryptor->HashPassword(CorrectString);
	const bool bIsEqualTrue = Encryptor->VerifyPassword(PassHash, CorrectString);
	const bool bIsEqualFalse = Encryptor->VerifyPassword(PassHash, IncorrectString);

	// Your test code here
	EXPECT_EQ(bIsEqualTrue == true, bIsEqualFalse == false);
}
