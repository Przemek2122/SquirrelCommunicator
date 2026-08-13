// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "ProjectEngine.h"
#include "BackendSettings.h"

#include "SQRLLEncryption.h"
#include "SQRLLIniObject.h"
#include "Logger/Logger.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

FBackendSettings::FBackendSettings()
    : MaxMessageSize(1024)
    , UnauthenticatedRequestsPerHour(300)
    , AuthenticatedRequestsPerHour(2000)
    , WebSocketIdleTimeoutSeconds(300)          // 5 minutes
    , VerboseLoggingLevel(1)                     // 1 = ERROR + WARN (default for production)
    , InviteDefaultMaxUses(1000)
    , InviteDefaultExpiresInSeconds(2592000)   // 30 days
    , InviteMaxExpiresInSeconds(31536000)       // 365 days (12 months)
    , MaxInvitesPerServer(10)
    , InviteAbuseMaxAttempts(10)
    , InviteAbuseWindowSeconds(120)
    , InviteAbuseBanDurationSeconds(3600)       // 1 hour
    , InviteCreateLimitPerHour(20)
    , InviteUseLimitPerHour(30)
    , RegisterAccountLimitPerHour(10)
    , MessageEncryptionSettings("", 32, 1, true)
{
}

void FBackendSettings::LoadBackendSettings()
{
    // Standalone: construct ini object directly with path (no IniManager needed)
    BackendSettingsIniObject = std::make_shared<SQRLLIniObject>("./Assets/Config/BackendSettings.ini");
    BackendSettingsIniObject->LoadIni();
    if (BackendSettingsIniObject->IsLoaded())
    {
        const FIniField MaxMessageSizeField = BackendSettingsIniObject->FindFieldByName("MaxMessageSize");
        if (MaxMessageSizeField.IsValid())
        {
            MaxMessageSize = MaxMessageSizeField.GetValueAsInt();
        }

        // --- Two-tier global rate limiting ---
        const FIniField UnauthenticatedRequestsPerHourField = BackendSettingsIniObject->FindFieldByName("UnauthenticatedRequestsPerHour");
        if (UnauthenticatedRequestsPerHourField.IsValid())
        {
            UnauthenticatedRequestsPerHour = UnauthenticatedRequestsPerHourField.GetValueAsInt();
        }

        const FIniField AuthenticatedRequestsPerHourField = BackendSettingsIniObject->FindFieldByName("AuthenticatedRequestsPerHour");
        if (AuthenticatedRequestsPerHourField.IsValid())
        {
            AuthenticatedRequestsPerHour = AuthenticatedRequestsPerHourField.GetValueAsInt();
        }

        // --- WebSocket settings ---
        const FIniField WebSocketIdleTimeoutSecondsField = BackendSettingsIniObject->FindFieldByName("WebSocketIdleTimeoutSeconds");
        if (WebSocketIdleTimeoutSecondsField.IsValid())
        {
            WebSocketIdleTimeoutSeconds = WebSocketIdleTimeoutSecondsField.GetValueAsInt();
        }

        // --- Logging settings ---
        const FIniField VerboseLoggingLevelField = BackendSettingsIniObject->FindFieldByName("VerboseLoggingLevel");
        if (VerboseLoggingLevelField.IsValid())
        {
            VerboseLoggingLevel = VerboseLoggingLevelField.GetValueAsInt();
        }
        Logger::SetVerboseLevel(VerboseLoggingLevel);

        const FIniField InviteDefaultMaxUsesField = BackendSettingsIniObject->FindFieldByName("InviteDefaultMaxUses");
        if (InviteDefaultMaxUsesField.IsValid())
        {
            InviteDefaultMaxUses = InviteDefaultMaxUsesField.GetValueAsInt();
        }

        const FIniField InviteDefaultExpiresInSecondsField = BackendSettingsIniObject->FindFieldByName("InviteDefaultExpiresInSeconds");
        if (InviteDefaultExpiresInSecondsField.IsValid())
        {
            InviteDefaultExpiresInSeconds = InviteDefaultExpiresInSecondsField.GetValueAsInt();
        }

        const FIniField InviteMaxExpiresInSecondsField = BackendSettingsIniObject->FindFieldByName("InviteMaxExpiresInSeconds");
        if (InviteMaxExpiresInSecondsField.IsValid())
        {
            InviteMaxExpiresInSeconds = InviteMaxExpiresInSecondsField.GetValueAsInt();
        }

        const FIniField MaxInvitesPerServerField = BackendSettingsIniObject->FindFieldByName("MaxInvitesPerServer");
        if (MaxInvitesPerServerField.IsValid())
        {
            MaxInvitesPerServer = MaxInvitesPerServerField.GetValueAsInt();
        }

        // --- Invite abuse protection ---
        const FIniField InviteAbuseMaxAttemptsField = BackendSettingsIniObject->FindFieldByName("InviteAbuseMaxAttempts");
        if (InviteAbuseMaxAttemptsField.IsValid())
        {
            InviteAbuseMaxAttempts = InviteAbuseMaxAttemptsField.GetValueAsInt();
        }

        const FIniField InviteAbuseWindowSecondsField = BackendSettingsIniObject->FindFieldByName("InviteAbuseWindowSeconds");
        if (InviteAbuseWindowSecondsField.IsValid())
        {
            InviteAbuseWindowSeconds = InviteAbuseWindowSecondsField.GetValueAsInt();
        }

        const FIniField InviteAbuseBanDurationSecondsField = BackendSettingsIniObject->FindFieldByName("InviteAbuseBanDurationSeconds");
        if (InviteAbuseBanDurationSecondsField.IsValid())
        {
            InviteAbuseBanDurationSeconds = InviteAbuseBanDurationSecondsField.GetValueAsInt();
        }

        // --- Invite hourly rate limits ---
        const FIniField InviteCreateLimitPerHourField = BackendSettingsIniObject->FindFieldByName("InviteCreateLimitPerHour");
        if (InviteCreateLimitPerHourField.IsValid())
        {
            InviteCreateLimitPerHour = InviteCreateLimitPerHourField.GetValueAsInt();
        }

        const FIniField InviteUseLimitPerHourField = BackendSettingsIniObject->FindFieldByName("InviteUseLimitPerHour");
        if (InviteUseLimitPerHourField.IsValid())
        {
            InviteUseLimitPerHour = InviteUseLimitPerHourField.GetValueAsInt();
        }

        // --- Registration rate limiting ---
        const FIniField RegisterAccountLimitPerHourField = BackendSettingsIniObject->FindFieldByName("RegisterAccountLimitPerHour");
        if (RegisterAccountLimitPerHourField.IsValid())
        {
            RegisterAccountLimitPerHour = RegisterAccountLimitPerHourField.GetValueAsInt();
        }

        // --- Message encryption key: read file path from INI, load key from file ---
        const FIniField MessageEncryptionKeyFileField = BackendSettingsIniObject->FindFieldByName("MessageEncryptionKeyFile");
        if (MessageEncryptionKeyFileField.IsValid())
        {
            MessageEncryptionKeyFilePath = MessageEncryptionKeyFileField.GetValueAsString();
        }

        // Load the encryption key from file or environment
        LoadMessageEncryptionKey();
    }
    else
    {
        LOG_ERROR("Backend settings are missing!");
    }
}

void FBackendSettings::LoadMessageEncryptionKey()
{
    // Environment variable takes priority over file-based config
    const char* EnvKey = std::getenv("SQRLL_MESSAGE_ENCRYPTION_KEY");
    if (EnvKey != nullptr && EnvKey[0] != '\0')
    {
        MessageEncryptionKey = std::string(EnvKey);
        LOG_STATE("✅ Message encryption key loaded from environment variable MESSAGE_ENCRYPTION_KEY. Message encryption is ENABLED.");
        return;
    }

    // If no file path is configured, encryption is disabled
    if (MessageEncryptionKeyFilePath.empty())
    {
        LOG_ERROR("❌ MessageEncryptionKeyFile is empty — at-rest message encryption is DISABLED. "
                  "All messages will be stored as plaintext in the database!");
        return;
    }

    // Resolve the path: if relative, make it relative to ./Assets/Config/
    std::string ResolvedPath = MessageEncryptionKeyFilePath;
    if (!std::filesystem::path(ResolvedPath).is_absolute())
    {
        ResolvedPath = "./Assets/Config/" + MessageEncryptionKeyFilePath;
    }

    // Normalize the path (resolve .. and . components)
    std::error_code Ec;
    std::string NormalizedPath = std::filesystem::canonical(
        std::filesystem::path(ResolvedPath).parent_path(), Ec).string();
    if (!Ec)
    {
        NormalizedPath += "/" + std::filesystem::path(ResolvedPath).filename().string();
    }
    else
    {
        NormalizedPath = ResolvedPath;
    }

    // Check if the key file exists
    if (std::filesystem::exists(NormalizedPath))
    {
        std::ifstream KeyFile(NormalizedPath);
        if (KeyFile.is_open())
        {
            std::string Line;
            if (std::getline(KeyFile, Line))
            {
                // Trim whitespace
                Line.erase(Line.begin(), std::find_if(Line.begin(), Line.end(),
                    [](unsigned char ch) { return !std::isspace(ch); }));
                Line.erase(std::find_if(Line.rbegin(), Line.rend(),
                    [](unsigned char ch) { return !std::isspace(ch); }).base(), Line.end());

                if (!Line.empty())
                {
                    MessageEncryptionKey = Line;
                    LOG_INFO("✅ Message encryption key loaded from file: " << NormalizedPath << ". "
                             "At-rest message encryption is ENABLED.");
                    return;
                }
            }
            // File exists but is empty or whitespace-only
            LOG_ERROR("❌ Message encryption key file exists but is empty: " << NormalizedPath << ". "
                      "At-rest message encryption is DISABLED. All messages will be stored as plaintext!");
            return;
        }
        else
        {
            LOG_ERROR("❌ Cannot open message encryption key file: " << NormalizedPath << ". "
                      "At-rest message encryption is DISABLED. All messages will be stored as plaintext!");
            return;
        }
    }

    // Key file does not exist — encryption is disabled
    LOG_ERROR("❌ Message encryption key file not found: " << NormalizedPath << ". "
              "At-rest message encryption is DISABLED. All messages will be stored as plaintext!");
}

std::string FBackendSettings::EncryptMessage(const std::string& Plaintext) const
{
    if (MessageEncryptionKey.empty())
    {
        // Encryption disabled — store as plaintext (backward compatible)
        return Plaintext;
    }

    try
    {
        return SQRLLEncryption::ToBaseN(SQRLLEncryption::Encrypt(Plaintext, MessageEncryptionKey, MessageEncryptionSettings), SQRLLPredefinedCharsets::BASE64);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Message encryption failed: " << e.what() << " — falling back to plaintext");
        return Plaintext;
    }
}

std::string FBackendSettings::DecryptMessage(const std::string& Ciphertext, const EMessageEncryptionStatus Status) const
{
    // If the DB says this message was NOT encrypted, return the raw text as-is.
    // This is the new canonical check — no magic-word heuristic needed.
    if (Status == EMessageEncryptionStatus::Unencrypted)
    {
        return Ciphertext;
    }

    // DB says encrypted, but no key loaded → should never happen, return as-is defensively
    if (MessageEncryptionKey.empty())
    {
        LOG_ERROR("Message is marked as encrypted in DB but no encryption key is loaded — returning raw data");
        return Ciphertext;
    }

    try
    {
        std::string Decrypted = SQRLLEncryption::Decrypt(SQRLLEncryption::FromBaseN(Ciphertext, SQRLLPredefinedCharsets::BASE64), MessageEncryptionKey, MessageEncryptionSettings);
        if (Decrypted.empty() && !Ciphertext.empty())
        {
            // Decryption failed (wrong key or tampered data) — return as-is to avoid data loss
            LOG_ERROR("Message decryption failed (key mismatch or corruption) — returning raw data");
            return Ciphertext;
        }
        return Decrypted;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Message decryption error: " << e.what() << " — returning raw data");
        return Ciphertext;
    }
}
