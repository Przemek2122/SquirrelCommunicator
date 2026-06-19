// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"
#include <shared_mutex>
#include <string>
#include <thread>

/** Structure for transfer token manager */
struct FTransferTokenData
{
    std::string Token;
    Uint64 ExpirationTime;
};

/**
 * Manager for keeping transfer tokens of integration login using token create / redeem
 */
class FTransferTokenManager
{
public:
    FTransferTokenManager();

    void Init();
    void PostSecondTick();

    void AsyncClearOldTransferTokens();

    std::string CreateTransferToken(Uint64 UserId);
    Uint64 GetUserIdFromTransferToken(const std::string& Token);
    bool RemoveTransferToken(const std::string& Token);
    bool IsTransferTokenValid(const std::string& Token);

private:
    /** Map with transfer tokens */
    std::unordered_map<Uint64, FTransferTokenData> TransferTokensMap;
    std::unordered_map<std::string, Uint64> TransferTokenToIdMap;

    /** Mutex for TransferTokensMap */
    std::shared_mutex TransferTokensMapMutex;

    /** Last updated time in async work */
    Uint64 AsyncWorkLastTime;

    /** Time saved for performance */
    Uint64 CurrentTimeCached;

    /** Background worker thread */
    std::jthread WorkerThread;

};
