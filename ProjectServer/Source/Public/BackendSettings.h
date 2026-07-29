// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"

/** Class for global backend settings */
class FBackendSettings
{
public:
    FBackendSettings();

    void LoadBackendSettings();

    std::shared_ptr<FIniObject> GetBackendSettingsIni() const { return BackendSettingsIniObject; }

    /** Max number of letters per message */
    int32 GetMaxMessageSize() const { return MaxMessageSize; }

    // --- Global rate limiting ---

    /** Max total requests (REST + WebSocket) per IP per hour (default 5000) */
    int32 GetGlobalRequestsPerHour() const { return GlobalRequestsPerHour; }

    // --- Invite settings ---

    /** Default max number of uses per invite (0 = unlimited by uses) */
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

protected:
    /** Settings ini object */
    std::shared_ptr<FIniObject> BackendSettingsIniObject;

    /** Max number of letters per message */
    int32 MaxMessageSize;

    // --- Global rate limiting ---
    int32 GlobalRequestsPerHour;

    // --- Invite settings ---
    int32 InviteDefaultMaxUses;
    int32 InviteDefaultExpiresInSeconds;
    int32 InviteMaxExpiresInSeconds;
    int32 MaxInvitesPerServer;

    // --- Invite abuse protection settings ---
    int32 InviteAbuseMaxAttempts;
    int32 InviteAbuseWindowSeconds;
    int32 InviteAbuseBanDurationSeconds;
};
