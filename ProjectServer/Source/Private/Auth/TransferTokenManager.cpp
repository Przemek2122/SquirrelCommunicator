// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Auth/TransferTokenManager.h"
#include "Misc/EncryptionUtil.h"
#include "Threads/ThreadsManager.h"

static const char* TransferTokenManagerThreadName = "TransferTokenThread";

FTransferTokenManager::FTransferTokenManager()
    : AsyncWorkLastTime(0)
    , CurrentTimeCached(0)
    , TransferTokenThreadData(nullptr)
{
}

void FTransferTokenManager::Init()
{
	CurrentTimeCached = FUtil::GetSeconds();

    FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
    TransferTokenThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>(TransferTokenManagerThreadName);
    FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(TransferTokenThreadData->GetThread());
    if (GenericThread != nullptr)
    {
        GenericThread->SetShouldRemoveDoneJobs(false);
        GenericThread->AddTask([this]()
        {
            AsyncWork();
        });

        GenericThread->BeginAsyncWork();
    }
    else
    {
        LOG_ERROR("Failed to create transfer token manager thread");
    }
}

void FTransferTokenManager::PostSecondTick()
{
	CurrentTimeCached = FUtil::GetSeconds();
}

void FTransferTokenManager::AsyncWork()
{
    constexpr Uint64 TimeBetweenRuns = 2 * 60; // Time in seconds
    const Uint64 CurrentTime = CurrentTimeCached;

    if (AsyncWorkLastTime + TimeBetweenRuns > CurrentTime)
    {
        THREAD_WAIT_MS(1000);
    }
    else
    {
        AsyncWorkLastTime = CurrentTime;

        AsyncClearOldTransferTokens();
    }
}

void FTransferTokenManager::AsyncClearOldTransferTokens()
{
    std::vector<std::string> ExpiredTokens;

    {
        std::shared_lock<std::shared_mutex> ReadLock(TransferTokensMapMutex);
        const Uint64 CurrentTime = CurrentTimeCached;

        for (const auto& [UserId, TokenData] : TransferTokensMap)
        {
            if (TokenData.ExpirationTime <= CurrentTime)
            {
                ExpiredTokens.push_back(TokenData.Token);
            }
        }
    }

    if (!ExpiredTokens.empty())
    {
        std::unique_lock<std::shared_mutex> WriteLock(TransferTokensMapMutex);

        for (const std::string& Token : ExpiredTokens)
        {
            // We must double-check if the token still exists in the reverse map.
            // Another thread might have overwritten or deleted it between our read and write locks.
            auto TokenIt = TransferTokenToIdMap.find(Token);
            if (TokenIt != TransferTokenToIdMap.end())
            {
                const Uint64 UserId = TokenIt->second;

                // Safely erase from both maps
                TransferTokensMap.erase(UserId);
                TransferTokenToIdMap.erase(TokenIt);
            }
        }
    }
}

std::string FTransferTokenManager::CreateTransferToken(const Uint64 UserId)
{
    const std::string Salt = FEncryptionUtil::GenerateSecureSalt(64);
    const std::string SaltAsBase62 = FEncryptionUtil::ToBaseN_Irreversible(Salt, FPredefinedCharsets::BASE62);

    FTransferTokenData TransferTokenData;
    TransferTokenData.Token = SaltAsBase62;
    TransferTokenData.ExpirationTime = CurrentTimeCached + 60; // @TODO: Should be added to config instead of magic number

    {
        std::unique_lock<std::shared_mutex> Lock(TransferTokensMapMutex);

        TransferTokensMap[UserId] = TransferTokenData;
        TransferTokenToIdMap[SaltAsBase62] = UserId;
    }

    return SaltAsBase62;

}

Uint64 FTransferTokenManager::GetUserIdFromTransferToken(const std::string& Token)
{
    std::shared_lock<std::shared_mutex> Lock(TransferTokensMapMutex);

    auto TokenIt = TransferTokenToIdMap.find(Token);
    if (TokenIt == TransferTokenToIdMap.end())
    {
        return 0;
    }

    return TokenIt->second;
}

bool FTransferTokenManager::RemoveTransferToken(const std::string& Token)
{
    std::unique_lock<std::shared_mutex> Lock(TransferTokensMapMutex);

    // Find the token in the reverse map
    auto TokenIt = TransferTokenToIdMap.find(Token);
    if (TokenIt == TransferTokenToIdMap.end())
    {
        return false; // Token does not exist, nothing to invalidate
    }

    // Get the User ID associated with this token
    const Uint64 UserId = TokenIt->second;

    // Remove from both maps to keep them synchronized and prevent memory leaks
    TransferTokensMap.erase(UserId);
    TransferTokenToIdMap.erase(TokenIt);

    return true;
}

bool FTransferTokenManager::IsTransferTokenValid(const std::string& Token)
{
    std::shared_lock<std::shared_mutex> Lock(TransferTokensMapMutex);

    // Check if token exists in the reverse map
    auto TokenIt = TransferTokenToIdMap.find(Token);
    if (TokenIt == TransferTokenToIdMap.end())
    {
        return false;
    }

    const Uint64 UserId = TokenIt->second;

    // Verify in the main map
    auto UserIt = TransferTokensMap.find(UserId);
    if (UserIt != TransferTokensMap.end())
    {
        if (UserIt->second.Token == Token)
        {
            // Check expiration time against the cached current time
            return UserIt->second.ExpirationTime > CurrentTimeCached;
        }
    }

    return false;
}
