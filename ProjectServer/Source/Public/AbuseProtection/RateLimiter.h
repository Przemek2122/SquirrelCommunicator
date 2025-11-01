// Created by Przemys³aw Wiewióra 2020-2025
#pragma once

#include "CoreMinimal.h"
#include "Types/Mutex/Mutex.h"

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

	std::unordered_map<std::string, FRateLimit> RateLimitMap;
	FMutex RateLimitMutex;
	FMutex ClearMutex;
};

/**
 * Class ensuring user do not spam with register requests or with login attempts, very simple we count then and clear every x time.
 * Attempts are cleared every x time where x is value in config, we do not keep time of failed attempts as it would be more complex
 */
class FRateLimiter
{
public:
	FRateLimiter(const int32 InClearTime, const int32 InNumberOfAttemptsToBlock);
	~FRateLimiter();

	/** Check if we have user blocked */
	bool IsAddressBlocked(const std::string& InAddress);

	/** Add register or login attempt */
	void AddProtectedActionAttempt(const std::string& InAddress);

protected:
	FRateLimitObject IPAddressToLimits;
	int32 ClearTime;
	int32 NumberOfAttemptsToBlock;

};
