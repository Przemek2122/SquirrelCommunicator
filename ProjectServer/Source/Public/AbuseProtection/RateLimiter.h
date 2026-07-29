// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include <shared_mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <ctime>

#include "EngineCompat.h"

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

	std::unordered_map<std::string_view, FRateLimit> RateLimitMap;
	std::shared_mutex RateLimitMutex;
	std::mutex ClearMutex;
	bool bIsClearing;
};

/**
 * Class ensuring user do not spam with register requests or with login attempts, very simple we count then and clear every x time.
 * Attempts are cleared every x time where x is value in config, we do not keep time of failed attempts as it would be more complex
 *
 * Also manages invite abuse protection (IP-based rolling-window + ban system).
 */
class FRateLimiter
{
public:
	FRateLimiter(int32 InClearingTimeInMins, int32 InNumberOfAttemptsToBlock, int32 InNumberOfPasswordResetAttemptsToBlock,
		int32 InNumberOfServerOperationAttemptsToBlock, int32 InGlobalRequestsPerHour,
		int32 InInviteAbuseMaxAttempts, int32 InInviteAbuseWindowSeconds, int32 InInviteAbuseBanDurationSeconds);
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

	/** Check if an IP has exceeded the global request cap (all endpoints combined) */
	bool IsAddressGloballyBlocked(const std::string_view InAddress);

	/** Add a global request attempt (called for every REST + WebSocket message) */
	void AddGlobalRequestAttempt(const std::string_view InAddress);

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

	/** Object for global request cap (REST + WebSocket combined per IP) */
	FRateLimitObject GlobalRequestLimits;

	/** Time when we clear limits */
	std::chrono::minutes ClearingTimeInMins;

	/** How many attempts are needed to block */
	int32 NumberOfAttemptsToBlock;

	/** How many attempts are needed to block password reset */
	int32 NumberOfPasswordResetAttemptsToBlock;

	/** How many attempts are needed to block server operation */
	int32 NumberOfServerOperationAttemptsToBlock;

	/** Global cap: max requests per IP per ClearingTimeInMins (default 5000/hr) */
	int32 GlobalRequestsPerHour;

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

	/** Remove timestamps older than the rolling window from a vector of attempts */
	void PurgeOldInviteAttempts(std::vector<time_t>& Attempts, time_t Now) const;

private:
	/** Background worker thread */
	std::jthread WorkerThread;
	std::chrono::time_point<std::chrono::utc_clock> AsyncWorkLastTime;

};
