// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include "CoreMinimal.h"

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
	void AddAttempt(const std::string& InKey);
	bool IsBlockedKey(const std::string& InKey, const int32 NumberOfAttemptsToBlock);

	void Reset();

	std::unordered_map<std::string, FRateLimit> RateLimitMap;
	std::mutex RateLimitMutex;
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
	FRateLimiter(const int32 InClearingTimeInMins, const int32 InNumberOfAttemptsToBlock, const int32 InNumberOfPasswordResetAttemptsToBlock);
	~FRateLimiter();

	/** Check if we have user blocked */
	bool IsAddressBlocked(const std::string& InAddress);

	/** Add register or login attempt */
	void AddProtectedActionAttempt(const std::string& InAddress);

	/** Check if address can request password reset **/
	bool IsPasswordResetAddressBlocked(const std::string& InAddress);

	/** Add password reset attempt */
	void AddPasswordResetAttempt(const std::string& InAddress);

	void AsyncWork();
	void ResetRateLimits();

protected:
	/** Object for limiting access */
	FRateLimitObject DefaultIPAddressToLimits;

	/** Object for limiting access when using verify */
	FRateLimitObject VerifyIPAddressToLimits;

	/** Object for limiting access when using password reset */
	FRateLimitObject PasswordResetIPAddressToLimits;

	/** Time when we clear limits */
	std::chrono::minutes ClearingTimeInMins;

	/** How many attempts are needed to block */
	int32 NumberOfAttemptsToBlock;

	/** How many attempts are needed to block */
	int32 NumberOfPasswordResetAttemptsToBlock;

private:
	FThreadData* RateLimiterThreadData;
	std::chrono::time_point<std::chrono::utc_clock> AsyncWorkLastTime;

};
