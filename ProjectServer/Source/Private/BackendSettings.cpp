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

        // Load (or generate) the actual key
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
    const char* EnvKey = std::getenv("MESSAGE_ENCRYPTION_KEY");
    if (EnvKey != nullptr && EnvKey[0] != '\0')
    {
        MessageEncryptionKey = std::string(EnvKey);
        LOG_INFO("Message encryption key loaded from environment variable MESSAGE_ENCRYPTION_KEY.");
        return;
    }

    // If no file path is configured, encryption is disabled
    if (MessageEncryptionKeyFilePath.empty())
    {
        LOG_INFO("MessageEncryptionKeyFile is empty — at-rest message encryption disabled.");
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

    // Check if the key file already exists
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
                    LOG_INFO("Message encryption key loaded from: " << NormalizedPath);
                    return;
                }
            }
            // File exists but is empty or whitespace-only — generate new key
            LOG_WARN("Message encryption key file exists but is empty — generating new key.");
        }
        else
        {
            LOG_ERROR("Cannot open message encryption key file: " << NormalizedPath);
            return;
        }
    }

    // Key file does not exist (or is empty) — generate a new key
    LOG_INFO("Generating new message encryption key...");

    // Generate 20 random bytes → ~27 chars in BASE62
    const std::string RandomBytes = FEncryptionUtil::GenerateSecureSalt(20);
    const std::string NewKey = FEncryptionUtil::ToBaseN(RandomBytes, FPredefinedCharsets::BASE62);

    // Ensure the parent directory exists
    std::error_code MkdirEc;
    std::filesystem::create_directories(
        std::filesystem::path(NormalizedPath).parent_path(), MkdirEc);

    // Write the key with 0600 permissions set atomically at creation (owner read/write only)
    int Fd = ::open(NormalizedPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (Fd >= 0)
    {
        const ssize_t Written = ::write(Fd, NewKey.data(), NewKey.size());
        ::close(Fd);

        MessageEncryptionKey = NewKey;

        if (Written == static_cast<ssize_t>(NewKey.size()))
        {
            LOG_INFO("Message encryption key generated and saved to: " << NormalizedPath);
        }
        else
        {
            LOG_WARN("Message encryption key write may be incomplete for: " << NormalizedPath);
        }
    }
    else
    {
        LOG_ERROR("Failed to write message encryption key to: " << NormalizedPath);
        MessageEncryptionKey = NewKey;
        LOG_WARN("Message encryption key is in-memory only (could not persist to disk).");
    }
}