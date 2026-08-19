// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"

#include <chrono>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

/** Pending registration awaiting email verification. */
struct FPendingRegistration
{
    std::string UserMail;
    std::string UserName;
    std::string PasswordHash;
    std::string VerificationCode;
    std::chrono::system_clock::time_point ExpirationTime;

    bool IsValid() const { return !UserMail.empty() && !VerificationCode.empty(); }
};

/**
 * Manages the email verification step of the database registration flow.
 *
 * When a user submits the registration form, a 6 digit code is generated,
 * stored here together with the (already hashed) credentials, and sent by
 * email. The account itself is only created once the code is validated and
 * consumed. Integrations (Google / Microsoft) bypass this flow entirely.
 */
class FEmailVerificationManager
{
public:
    explicit FEmailVerificationManager(int32 InCodeAliveTimeMins = 30);

    /** Init async cleanup thread. */
    void Init();

    /**
     * Generate a verification code and store the pending registration.
     * Overwrites any previous pending registration for the same mail.
     */
    FPendingRegistration GenerateVerificationCode(const std::string& UserMail, const std::string& UserName, const std::string& PasswordHash);

    /** Return the pending registration if the code matches and is not expired. */
    FPendingRegistration ValidateCode(const std::string& UserMail, const std::string& Code);

    /** Remove a pending registration after successful completion. */
    void ConsumeCode(const std::string& UserMail, const std::string& Code);

    /** Remove expired pending registrations. */
    void AsyncCleanupExpired();

protected:
    /** Map of mail -> pending registration. */
    std::unordered_map<std::string, FPendingRegistration> MailToPendingRegistration;

    /** Mutex for MailToPendingRegistration. */
    std::shared_mutex PendingMutex;

    /** Background worker thread. */
    std::jthread WorkerThread;

    /** Time each code stays alive, in minutes. */
    int32 CodeAliveTimeMins;

    /** Time of last async cleanup (seconds since epoch). */
    Uint64 AsyncWorkLastTime;
};
