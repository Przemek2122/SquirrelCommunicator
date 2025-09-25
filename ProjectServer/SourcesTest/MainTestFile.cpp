#include <gtest/gtest.h>
#include "Misc/EncryptionManager.h"
#include "Misc/PasswordEncryptionBase.h"
#include "Public/Project.h"

// Sample test
TEST(Test, BasicEncryptionTest)
{
	FEncryptionManager EncryptionManager;
	const std::string PassHash = EncryptionManager.HashPassword("MyT4STStringu", EPasswordEncryptionAlgorithm::Argon2);
	const bool bIsEqualTrue = EncryptionManager.VerifyPassword(PassHash, "MySTStringu", EPasswordEncryptionAlgorithm::Argon2);
	const bool bIsEqualFalse = EncryptionManager.VerifyPassword(PassHash, "MyT4STStringu", EPasswordEncryptionAlgorithm::Argon2);

	// Your test code here
    EXPECT_EQ(bIsEqualTrue == true, bIsEqualFalse == false);
}
