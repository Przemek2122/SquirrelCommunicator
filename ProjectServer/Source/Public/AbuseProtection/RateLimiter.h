// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include <shared_mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <ctime>
#include <string>

#include "EngineCompat.h"

/*
 * Transparent hasher & equality for std::unordered_map<std::string, ...>
 * Enables heterogeneous lookup with std::string_view without constructing
 * a temporary std::string on every find() / contains() call.
 */
struct FTransparentStringHash
{
	using is_transparent = void; // enables heterogeneous lookup (C++20)

	size_t operator()(std::string_view sv) const noexcept
	{
		return std::hash<std::string_view>{}(sv);
	}

	size_t operator()(const std::string& s) const noexcept
	{
		return std::hash<std::string>{}(s);
	}
};

struct FTransparentStringEqual
{
	using is_transparent = void;

	bool operator()(std::string_view a, std::string_view b) const noexcept
	{
		return a == b;
	}
};

/** Assume reset is done by removing from map */
struct FRateLimit
{
public:
	FRateLimit();

	void AddAttempt();
	int32 GetAttemptCount() const { return AttemptCount.load(std::memory_order_relaxed); }

protected:
	/** Atomic to safely increment under shared_lock from multiple threads */
	std::atomic<int32> AttemptCount;
	
};

struct FRateLimitObject
{
public:
	void AddAttempt(const std::string_view& InKey);
	bool IsBlockedKey(const std::string_view& InKey, int32 NumberOfAttemptsToBlock);

	void Reset();

	std::unordered_map<std::string, FRateLimit, FTransparentStringHash, FTransparentStringEqual> RateLimitMap;
	std::shared_mutex RateLimitMutex;
};

/**
 * Class ensuring user do not spam with register requests or with login attempts, very simple we count then and clear every x time.
 * Attempts are cleared every x time where x is value in config, we do not keep time of failed attempts as it would be more complex
 *
 * Also manages invite abuse protection (IP-based rolling-window + ban system),
 * invite hourly rate limits (simple counter per clearing interval),
 * two-tier global rate limiting (unauthenticated per-IP + authenticated per-UserID),
 * and registration rate limiting (per-IP counter).
 */
class FRateLimiter
{
public:
	FRateLimiter(int32 InClearingTimeInMins, int32 InNumberOfAttemptsToBlock, int32 InNumberOfPasswordResetAttemptsToBlock,
		int32 InNumberOfServerOperationAttemptsToBlock,
		int32 InUnauthenticatedRequestsPerHour, int32 InAuthenticatedRequestsPerHour,
		int32 InInviteAbuseMaxAttempts, int32 InInviteAbuseWindowSeconds, int32 InInviteAbuseBanDurationSeconds,
		int32 InInviteCreateLimitPerHour, int32 InInviteUseLimitPerHour,
		int32 InRegisterAccountLimitPerHour);
	~FRateLimiter();

	/** Check if we have user blocked */
	bool IsAddressBlocked(const std::string_view InAddress);

	/** Add register or login attempt */
	void AddProtectedActionAttempt(const std::string_view InAddress);

	/** Check if address can request password reset **/
	bool IsPasswordResetAddressBlocked(const std::string_view InAddress);

	/** Add password reset attempt */
	void AddPasswordResetAttempt(const std::string_view InAddress);

	/** Check if address can perform server operation */
	bool IsServerOperationAddressBlocked(const std::string_view InAddress);

	/** Add server operation attempt */
	void AddServerOperationAttempt(const std::string_view InAddress);

	// --- Two-tier global rate limiting ---

	/** Tier 1: Check if an unauthenticated IP has exceeded the strict per-IP cap */
	bool IsUnauthenticatedIPBlocked(const std::string_view InAddress);

	/** Tier 1: Record an unauthenticated request for this IP */
	void AddUnauthenticatedIPAttempt(const std::string_view InAddress);

	/** Tier 2: Check if an authenticated UserID has exceeded the per-user cap */
	bool IsAuthenticatedUserBlocked(const std::string_view UserIdStr);

	/** Tier 2: Record an authenticated request for this UserID */
	void AddAuthenticatedUserAttempt(const std::string_view UserIdStr);

	// --- Invite create hourly rate limit ---

	/** Check if an IP has exceeded the invite creation rate limit */
	bool IsInviteCreateAddressBlocked(const std::string_view InAddress);

	/** Record an invite creation attempt */
	void AddInviteCreateAttempt(const std::string_view InAddress);

	// --- Invite use hourly rate limit ---

	/** Check if an IP has exceeded the invite use rate limit */
	bool IsInviteUseAddressBlocked(const std::string_view InAddress);

	/** Record an invite use attempt */
	void AddInviteUseAttempt(const std::string_view InAddress);

	// --- Registration rate limiting ---

	/** Check if an IP has exceeded the registration rate limit */
	bool IsRegisterAccountAddressBlocked(const std::string_view InAddress);

	/** Record a registration attempt */
	void AddRegisterAccountAttempt(const std::string_view InAddress);

	void ResetRateLimits();

	// --- Invite Abuse Protection (rolling-window + ban) ---

	/**
	 * Check if an IP is currently banned from using invite codes.
	 * @param Ip  The client IP address (owned string, stored long-term).
	 * @return    Seconds remaining in the ban, or 0 if not banned.
	 */
	Uint64 IsInviteAbuseBanned(const std::string& Ip) const;

	/**
	 * Record a failed invite attempt for an IP.
	 * If the failure count exceeds MaxAttempts within the Window, the IP is banned.
	 * @param Ip  The client IP address.
	 * @return    true if this attempt triggered a ban, false otherwise.
	 */
	bool AddInviteAbuseAttempt(const std::string& Ip);

	/**
	 * Clear all records for an IP (called on successful invite join).
	 * @param Ip  The client IP address.
	 */
	void ClearInviteAbuseForIp(const std::string& Ip);

	/**
	 * Periodic cleanup: remove expired bans and stale attempt records.
	 * Should be called every few minutes to prevent memory leaks.
	 */
	void PeriodicInviteAbuseCleanup();

protected:
	/**
	 * FRateLimitObject - Left as separate objects for maximum performance
	 * (This objects contains list of IP Addresses which can be really big)
	 */

	/** Object for limiting access */
	FRateLimitObject DefaultIPAddressToLimits;

	/** Object for limiting access when using verify */
	FRateLimitObject VerifyIPAddressToLimits;

	/** Object for limiting access when using password reset */
	FRateLimitObject PasswordResetIPAddressToLimits;

	/** Object for limiting access when using server operations */
	FRateLimitObject ServerOperationAddressToLimits;

	// --- Two-tier global rate limiting ---

	/** Tier 1: Strict per-IP limit for unauthenticated requests (default 300/hr) */
	FRateLimitObject UnauthenticatedIPLimits;

	/** Tier 2: Per-UserID limit for authenticated requests (default 2000/hr) */
	FRateLimitObject AuthenticatedUserLimits;

	/** Object for limiting invite creation per IP */
	FRateLimitObject InviteCreateLimits;

	/** Object for limiting invite use per IP */
	FRateLimitObject InviteUseLimits;

	/** Object for limiting new account registrations per IP */
	FRateLimitObject RegisterAccountLimits;

	/** Time when we clear limits */
	std::chrono::minutes ClearingTimeInMins;

	/** How many attempts are needed to block */
	int32 NumberOfAttemptsToBlock;

	/** How many attempts are needed to block password reset */
	int32 NumberOfPasswordResetAttemptsToBlock;

	/** How many attempts are needed to block server operation */
	int32 NumberOfServerOperationAttemptsToBlock;

	// --- Two-tier global rate limiting ---

	/** Tier 1: Max unauthenticated requests per IP per clearing interval (default 300/hr) */
	int32 UnauthenticatedRequestsPerHour;

	/** Tier 2: Max authenticated requests per UserID per clearing interval (default 2000/hr) */
	int32 AuthenticatedRequestsPerHour;

	/** Max invite creations per IP per clearing interval */
	int32 InviteCreateLimitPerHour;

	/** Max invite uses per IP per clearing interval */
	int32 InviteUseLimitPerHour;

	/** Max new account registrations per IP per clearing interval */
	int32 RegisterAccountLimitPerHour;

	// --- Invite Abuse Protection data ---

	/** Per-IP tracking data for invite abuse */
	struct FInviteAbuseRecord
	{
		/** Timestamps of failed attempts within the current rolling window */
		std::vector<time_t> FailedAttempts;

		/** Unix timestamp when the ban expires (0 = not banned) */
		time_t BanUntil = 0;
	};

	/** Map of IP -> invite abuse attempt records (long-lived, uses owned strings) */
	std::unordered_map<std::string, FInviteAbuseRecord> InviteAbuseRecords;

	/** Mutex for thread-safe access to invite abuse data */
	mutable std::shared_mutex InviteAbuseMutex;

	/** Configuration for invite abuse (set at construction, immutable after) */
	const int32 InviteAbuseMaxAttempts;
	const int32 InviteAbuseWindowSeconds;
	const int32 InviteAbuseBanDurationSeconds;

	/**
	 * Purge timestamps older than the rolling window from a vector of attempts.
	 * Uses jfalcou/eve SIMD library for portable auto-vectorization
	 * (AVX-512, AVX2, NEON, SVE — best ISA selected at compile time).
	 */
	void PurgeOldInviteAttempts(std::vector<time_t>& Attempts, time_t Now) const;

private:
	/** Background worker thread */
	std::jthread WorkerThread;
	std::chrono::time_point<std::chrono::utc_clock> AsyncWorkLastTime;

};
