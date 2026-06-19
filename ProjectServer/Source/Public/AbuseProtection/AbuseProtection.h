// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025
#pragma once

#include "EngineCompat.h"
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
	explicit FAbuseProtection(const FBackendSettings* InBackendSettings);

	bool IsAddressBlocked(const std::string_view InAddress) const;
	void AddRateLimitedAttempt(const std::string_view InAddress) const;

	bool CanAddressRequestPasswordReset(const std::string_view InAddress) const;
	void AddPasswordResetAttempt(const std::string_view InAddress) const;

	bool CanAddressRequestCreateRoom(const std::string_view InAddress) const;
	void AddCreateRoomAttempt(const std::string_view InAddress) const;

	CUnorderedMap<std::string, std::string> GetCORHeaders() const;

protected:
	std::unique_ptr<FCORPolicy> CORPolicyPtr;
	std::unique_ptr<FRateLimiter> RateLimiter;
};
