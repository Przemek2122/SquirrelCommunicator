// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include "EngineCompat.h"
#include "AbuseProtection/CORPolicy.h"
#include "AbuseProtection/RateLimiter.h"

/**
 * Class used for abuse protection
 * Supports: Rate limiting, blocking address, invite abuse protection
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

	/** Check if an IP has exceeded the global request cap (5000/hr, all endpoints) */
	bool IsAddressGloballyBlocked(const std::string_view InAddress) const;

	/** Record a global request attempt (called on every REST + WebSocket message) */
	void AddGlobalRequestAttempt(const std::string_view InAddress) const;

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

	CUnorderedMap<std::string, std::string> GetCORHeaders() const;

protected:
	std::unique_ptr<FCORPolicy> CORPolicyPtr;
	std::unique_ptr<FRateLimiter> RateLimiter;
};
