// Created by https://www.linkedin.com/in/przemek2122/ 2026 https://github.com/Przemek2122/Engine

#include "Managers/PasswordResetManager.h"

#include "ProjectEngine.h"
#include "Auth/UserManager.h"
#include "ThreadCompat.h"

#include <random>

FPasswordResetManager::FPasswordResetManager(int32 InTimeInMinsForTokenToBeAlive)
    : TimeInMinsForTokenToBeAlive(InTimeInMinsForTokenToBeAlive)
    , AsyncWorkLastTime(0)
{
}

void FPasswordResetManager::Init()
{
    // Initially skip, there is no chance we will somehow get tokens
    AsyncWorkLastTime = FUtil::GetSeconds();

    WorkerThread = std::jthread([this](std::stop_token stoken)
    {
        constexpr Uint64 TimeToWaitBetweenRuns = 60; // Time to wait (in seconds)

        while (!stoken.stop_requested())
        {
            const Uint64 CurrentTime = FUtil::GetSeconds();

            if (CurrentTime > (AsyncWorkLastTime + TimeToWaitBetweenRuns))
            {
                AsyncCleanupTokens();
                AsyncWorkLastTime = FUtil::GetSeconds();
            }

            // Sleep 1 second between checks
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

FPasswordResetStruct FPasswordResetManager::GenerateResetToken(const std::string& UserMail)
{
    // Set of chars 0-9, A-F
    static std::array<char, 16> RandomBytes = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    static int32 TokenLength = 6;

    std::vector<char> RandomToken;
    RandomToken.reserve(TokenLength);

    for (int32 i = 0; i < TokenLength; ++i)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int32> dist(0, 15);
        const int32 RandomIndex = dist(gen);
        RandomToken.push_back(RandomBytes[RandomIndex]);
    }

    // Make token as string
    const std::string TokenString(RandomToken.begin(), RandomToken.end());

    // Make struct
    FPasswordResetStruct ResetStruct = { UserMail, TokenString };
    ResetStruct.TokenExpirationTime = std::chrono::system_clock::now() + std::chrono::minutes(TimeInMinsForTokenToBeAlive);

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

void FPasswordResetManager::InvalidateToken(const std::string& ResetToken)
{
    // Mutex unique lock
    std::unique_lock<std::shared_mutex> Lock(TokenToStructureMapMutex);

    TokenToStructureMap.erase(ResetToken);
}

void FPasswordResetManager::AsyncCleanupTokens()
{
    // Mutex unique lock
    std::unique_lock<std::shared_mutex> Lock(TokenToStructureMapMutex);

    // Iterate map to find and remove outdated tokens
    for (auto It = TokenToStructureMap.begin(); It != TokenToStructureMap.end();)
    {
        // Check if expired
        if (It->second.TokenExpirationTime < std::chrono::system_clock::now())
        {
            TokenToStructureMap.erase(It++);
            continue;
        }

        ++It;
    }
}
