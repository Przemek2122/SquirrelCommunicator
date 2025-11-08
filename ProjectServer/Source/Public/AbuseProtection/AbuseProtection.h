// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include "CoreMinimal.h"
#include "AbuseProtection/CORPolicy.h"
#include "AbuseProtection/RateLimiter.h"

/**
 * Class used for abuse protection
 * Supports: Rate limiting, blocking address
 * Currently also used for COR Headers
 */
class FAbuseProtection
{
public:
	explicit FAbuseProtection(FBackendSettings* InBackendSettings);

	bool IsAddressBlocked(const std::string& InAddress);
	void AddRateLimitedAttempt(const std::string& InAddress);

	CUnorderedMap<std::string, std::string> GetCORHeaders() const;

protected:
	std::unique_ptr<FCORPolicy> CORPolicyPtr;
	std::unique_ptr<FRateLimiter> RateLimiter;
};
