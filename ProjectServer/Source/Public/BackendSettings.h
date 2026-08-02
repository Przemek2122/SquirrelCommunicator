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

protected:
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
};
