// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include <shared_mutex>
#include <thread>

#include "EngineCompat.h"

/** Assume reset is done by removing from map */
struct FRateLimit
{
public:
	FRateLimit();

	void AddAttempt();
	int32 GetAttemptCount() const { return AttemptCount; }

protected:
	int32 AttemptCount;
	
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
 */
class FRateLimiter
{
public:
	FRateLimiter(int32 InClearingTimeInMins, int32 InNumberOfAttemptsToBlock, int32 InNumberOfPasswordResetAttemptsToBlock,
		int32 InNumberOfServerOperationAttemptsToBlock);
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

	void ResetRateLimits();

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

	/** Time when we clear limits */
	std::chrono::minutes ClearingTimeInMins;

	/** How many attempts are needed to block */
	int32 NumberOfAttemptsToBlock;

	/** How many attempts are needed to block password reset */
	int32 NumberOfPasswordResetAttemptsToBlock;

	/** How many attempts are needed to block server operation */
	int32 NumberOfServerOperationAttemptsToBlock;

private:
	/** Background worker thread */
	std::jthread WorkerThread;
	std::chrono::time_point<std::chrono::utc_clock> AsyncWorkLastTime;

};
