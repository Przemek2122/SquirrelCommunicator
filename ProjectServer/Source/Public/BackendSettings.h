// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"
#include "SQRLLEncryption.h"

/** Class for global backend settings */
class FBackendSettings
{
public:
    FBackendSettings();

    void LoadBackendSettings();

    std::shared_ptr<FIniObject> GetBackendSettingsIni() const { return BackendSettingsIniObject; }

    /** Max number of letters per message */
    int32 GetMaxMessageSize() const { return MaxMessageSize; }

    // --- Two-tier global rate limiting ---

    /** Tier 1: Max unauthenticated (no token) requests per IP per hour (default 300) */
    int32 GetUnauthenticatedRequestsPerHour() const { return UnauthenticatedRequestsPerHour; }

    /** Tier 2: Max authenticated (with token) requests per UserID per hour (default 2000) */
    int32 GetAuthenticatedRequestsPerHour() const { return AuthenticatedRequestsPerHour; }

    // --- Invite settings ---

    /** Default max number of uses per invite */
    int32 GetInviteDefaultMaxUses() const { return InviteDefaultMaxUses; }

    /** Default invite expiration time in seconds */
    int32 GetInviteDefaultExpiresInSeconds() const { return InviteDefaultExpiresInSeconds; }

    /** Maximum allowed invite expiration time in seconds (hard cap, 12 months) */
    int32 GetInviteMaxExpiresInSeconds() const { return InviteMaxExpiresInSeconds; }

    /** Maximum number of active (non-expired) invites per server (default 10) */
    int32 GetMaxInvitesPerServer() const { return MaxInvitesPerServer; }

    // --- Invite abuse protection settings ---

    /** Max failed invite attempts before an IP is banned (default 10) */
    int32 GetInviteAbuseMaxAttempts() const { return InviteAbuseMaxAttempts; }

    /** Rolling window in seconds for counting failed invite attempts (default 120) */
    int32 GetInviteAbuseWindowSeconds() const { return InviteAbuseWindowSeconds; }

    /** Ban duration in seconds for invite abuse (default 3600 = 1 hour) */
    int32 GetInviteAbuseBanDurationSeconds() const { return InviteAbuseBanDurationSeconds; }

    // --- Invite hourly rate limits (simple per-IP counter, separate from rolling-window ban) ---

    /** Max invite creation requests per IP per hour (default 20) */
    int32 GetInviteCreateLimitPerHour() const { return InviteCreateLimitPerHour; }

    /** Max invite use attempts per IP per hour (default 30) */
    int32 GetInviteUseLimitPerHour() const { return InviteUseLimitPerHour; }

    // --- Registration rate limiting ---

    /** Max new account registrations per IP per hour (default 10) */
    int32 GetRegisterAccountLimitPerHour() const { return RegisterAccountLimitPerHour; }

    // --- Message encryption ---

    /** Path to the encryption key file (from INI). Relative to Assets/Config or absolute. */
    const std::string& GetMessageEncryptionKeyFilePath() const { return MessageEncryptionKeyFilePath; }

    /** Server-side message encryption key (BASE62). Empty = at-rest encryption disabled. */
    const std::string& GetMessageEncryptionKey() const { return MessageEncryptionKey; }

    /** Whether server-side at-rest message encryption is active */
    bool IsEncryptionEnabled() const { return !MessageEncryptionKey.empty(); }

    /** Get the SQRLL encryption settings for message at-rest encryption */
    const SQRLLSettings& GetEncryptionSettings() const { return MessageEncryptionSettings; }

    /** Encrypt a plaintext message for at-rest DB storage. Returns plaintext if encryption disabled. */
    [[nodiscard]] std::string EncryptMessage(const std::string& Plaintext) const;

    /** Decrypt a ciphertext message from DB storage. Returns ciphertext unchanged if encryption disabled. */
    [[nodiscard]] std::string DecryptMessage(const std::string& Ciphertext) const;

protected:
    /** Load the message encryption key from disk or environment variable */
    void LoadMessageEncryptionKey();

    /** Settings ini object */
    std::shared_ptr<FIniObject> BackendSettingsIniObject;

    /** Max number of letters per message */
    int32 MaxMessageSize;

    // --- Two-tier global rate limiting ---
    int32 UnauthenticatedRequestsPerHour;
    int32 AuthenticatedRequestsPerHour;

    // --- Invite settings ---
    int32 InviteDefaultMaxUses;
    int32 InviteDefaultExpiresInSeconds;
    int32 InviteMaxExpiresInSeconds;
    int32 MaxInvitesPerServer;

    // --- Invite abuse protection settings ---
    int32 InviteAbuseMaxAttempts;
    int32 InviteAbuseWindowSeconds;
    int32 InviteAbuseBanDurationSeconds;

    // --- Invite hourly rate limits ---
    int32 InviteCreateLimitPerHour;
    int32 InviteUseLimitPerHour;

    // --- Registration rate limiting ---
    int32 RegisterAccountLimitPerHour;

    // --- Message encryption ---
    std::string MessageEncryptionKeyFilePath;
    std::string MessageEncryptionKey;

    /** SQRLL encryption settings for at-rest message storage */
    SQRLLSettings MessageEncryptionSettings;
};
