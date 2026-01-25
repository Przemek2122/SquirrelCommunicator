// Created by https://www.linkedin.com/in/przemek2122/ 2026 https://github.com/Przemek2122/Engine

#pragma once
#include <shared_mutex>

struct FPasswordResetStruct
{
    std::string UserMailForReset;
    std::string ResetToken;
    std::chrono::system_clock::time_point TokenExpirationTime;
};

/**
 * Class FPasswordResetManager is responsible for managing password reset workflows.
 * 1. It handles token generation - this token is used to reset password on UI.
 * 2. Next User gets this token to his E-Mail provided on UI.
 * 3. User uses this token to reset password on UI.
 * 4. Backend verifies token.
 * 5. If token is valid, password is updated.
 */
class FPasswordResetManager
{
public:
    FPasswordResetManager(int32 InTimeInMinsForTokenToBeAlive = 10);

    /** Init async thread */
    void Init();

    /** Generate token for user password reset. */
    FPasswordResetStruct GenerateResetToken(const std::string& UserMail);

    /** Check if token is valid and associated with provided user email. */
    bool ValidateResetToken(const std::string& UserMail, const std::string& ResetToken);

    /** Do actual password reset. Make sure to use ValidateResetToken! */
    bool UpdatePassword(const std::string& UserMail, const std::string& NewPassword);

    /** Invalidate token. */
    void InvalidateToken(const std::string& ResetToken);

    /** Async function collecting all tokens and removing outdated */
    void AsyncCleanupTokens();

    /** Update waiting time */
    void AsyncUpdateWaitingTime();

protected:
    /** Map of tokens to their associated reset structure. */
    std::unordered_map<std::string, FPasswordResetStruct> TokenToStructureMap;

    /** Mutex for TokenToStructureMap. */
    std::shared_mutex TokenToStructureMapMutex;

    /** Thread data */
    FThreadData* TokenManagerThreadData;

    /** Time for each token generate while it's active */
    int32 TimeInMinsForTokenToBeAlive;

    /** Time of last async work */
    Uint64 AsyncWorkLastTime;

};
