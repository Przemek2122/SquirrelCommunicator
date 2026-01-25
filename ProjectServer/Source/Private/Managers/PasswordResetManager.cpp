// Created by https://www.linkedin.com/in/przemek2122/ 2026 https://github.com/Przemek2122/Engine

#include "Managers/PasswordResetManager.h"

#include "ProjectEngine.h"
#include "Auth/UserManager.h"

FPasswordResetStruct FPasswordResetManager::GenerateResetToken(const std::string& UserMail)
{
    // Set of chars 0-9, A-F
    static std::array<char, 16> RandomBytes = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    static int32 TokenLength = 6;

    std::vector<char> RandomToken;
    RandomToken.reserve(TokenLength);

    for (int32 i = 0; i < TokenLength; ++i)
    {
        RandomToken.push_back(RandomBytes[rand() % RandomBytes.size()]);
    }

    // Make token as string
    const std::string TokenString(RandomToken.begin(), RandomToken.end());

    // Make struct
    FPasswordResetStruct ResetStruct = { UserMail, TokenString };

    // Mutex unique lock
    std::unique_lock<std::shared_mutex> Lock(TokenToStructureMapMutex);

    // Add token to map
    TokenToStructureMap[TokenString] = ResetStruct;

    return ResetStruct;
}

bool FPasswordResetManager::ValidateResetToken(const std::string& UserMail, const std::string& ResetToken)
{
    bool bTokenMatch = false;

    // Mutex shared lock
    std::shared_lock<std::shared_mutex> Lock(TokenToStructureMapMutex);

    if (TokenToStructureMap.contains(ResetToken) && TokenToStructureMap[ResetToken].UserMailForReset == UserMail)
    {
        bTokenMatch = true;
    }

    return bTokenMatch;
}

bool FPasswordResetManager::UpdatePassword(const std::string& UserMail, const std::string& NewPassword)
{
    FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
    if (ProjectEngine != nullptr)
    {
        FUserManager* UserManager = ProjectEngine->GetUserManager();

        std::shared_ptr<FUser> User = UserManager->FindUserByMail(UserMail);

        if (User != nullptr)
        {
            const EUpdateUserPasswordStatus Result = UserManager->OverrideUserPassword(User->GetUserId(), NewPassword);

            return Result == EUpdateUserPasswordStatus::Successful;
        }
    }

    return false;
}

void FPasswordResetManager::InvalidateToken(const std::string& UserMail, const std::string& ResetToken)
{
}
