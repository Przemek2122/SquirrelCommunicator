// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "ProjectEngine.h"
#include "BackendSettings.h"
#include "SQRLLIniObject.h"
#include "Logger/Logger.h"

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
    }
    else
    {
        LOG_ERROR("Backend settings are missing!");
    }
}
