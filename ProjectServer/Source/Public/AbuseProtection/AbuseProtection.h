// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include "EngineCompat.h"
#include "AbuseProtection/CORPolicy.h"
#include "AbuseProtection/RateLimiter.h"

/**
 * Class used for abuse protection
 * Supports: Rate limiting, blocking address, invite abuse protection, invite hourly rate limits,
 * two-tier global rate limiting (unauthenticated per-IP + authenticated per-UserID)
 * Currently also used for COR Headers
 */
class FAbuseProtection
{
public:
	explicit FAbuseProtection(const FBackendSettings* InBackendSettings);

	bool IsAddressBlocked(const std::string_view InAddress) const;
	void AddRateLimitedAttempt(const std::string_view InAddress) const;

	bool CanAddressRequestPasswordReset(const std::string_view InAddress) const;
	void AddPasswordResetAttempt(const std::string_view InAddress) const;

	bool CanAddressRequestCreateServer(const std::string_view InAddress) const;
	void AddCreateServerAttempt(const std::string_view InAddress) const;

	// --- Two-tier global rate limiting ---

	/** Tier 1: Check if an unauthenticated IP has exceeded the strict per-IP cap (default 300/hr) */
	bool IsUnauthenticatedIPBlocked(const std::string_view InAddress) const;

	/** Tier 1: Record an unauthenticated request for this IP */
	void AddUnauthenticatedIPAttempt(const std::string_view InAddress) const;

	/** Tier 2: Check if an authenticated UserID has exceeded the per-user cap (default 2000/hr) */
	bool IsAuthenticatedUserBlocked(const std::string_view UserIdStr) const;

	/** Tier 2: Record an authenticated request for this UserID */
	void AddAuthenticatedUserAttempt(const std::string_view UserIdStr) const;

	// --- Invite Abuse Protection (rolling-window + ban) ---

	/**
	 * Check if an IP is currently banned from using invite codes.
	 * @return Seconds remaining in the ban, or 0 if not banned.
	 */
	Uint64 IsInviteAbuseBanned(const std::string& Ip) const;

	/** Record a failed invite attempt. Returns true if this triggered a ban. */
	bool AddInviteAbuseAttempt(const std::string& Ip) const;

	/** Clear all records for an IP (called on successful invite join). */
	void ClearInviteAbuseForIp(const std::string& Ip) const;

	/** Periodic cleanup of expired bans and stale records. */
	void PeriodicInviteAbuseCleanup() const;

	// --- Invite Hourly Rate Limits (simple per-IP counter, configurable in INI) ---

	/** Check if an IP can create an invite (has not exceeded hourly limit, default 20/hr) */
	bool CanAddressCreateInvite(const std::string_view InAddress) const;

	/** Record an invite creation attempt for the given IP */
	void AddCreateInviteAttempt(const std::string_view InAddress) const;

	/** Check if an IP can attempt to use an invite (has not exceeded hourly limit, default 30/hr) */
	bool CanAddressUseInvite(const std::string_view InAddress) const;

	/** Record an invite use attempt for the given IP */
	void AddUseInviteAttempt(const std::string_view InAddress) const;

	[[nodiscard]] CUnorderedMap<std::string, std::string> GetCORHeaders() const;

protected:
	std::unique_ptr<FCORPolicy> CORPolicyPtr;
	std::unique_ptr<FRateLimiter> RateLimiter;
};
