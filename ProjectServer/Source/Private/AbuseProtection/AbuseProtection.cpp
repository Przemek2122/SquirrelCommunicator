#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "SQRLLIniObject.h"

FAbuseProtection::FAbuseProtection(const FBackendSettings* InBackendSettings)
	: CORPolicyPtr(std::make_unique<FCORPolicy>())
{
	const std::shared_ptr<FIniObject> BackendSettingsIni = InBackendSettings->GetBackendSettingsIni();
	if (BackendSettingsIni->IsLoaded())
	{
		LOG_DEBUG("BackendSettingsIni number of fields: '" << BackendSettingsIni->GetNumberOfFields() << "'.");

		const FIniField RateLimitTimeToClearInMinsField = BackendSettingsIni->FindFieldByName("RateLimitTimeToClearInMins");
		const FIniField RateLimitNumberPerIPField = BackendSettingsIni->FindFieldByName("RateLimitNumberPerIP");
		const FIniField RateLimitPasswordField = BackendSettingsIni->FindFieldByName("PasswordRateLimitNumberPerIP");
		const FIniField RateLimitCreateServerField = BackendSettingsIni->FindFieldByName("CreateRoomRateLimitNumberPerIP");

		// Read two-tier global rate limits from BackendSettings (already parsed)
		const int32 UnauthenticatedRequestsPerHour = InBackendSettings->GetUnauthenticatedRequestsPerHour();
		const int32 AuthenticatedRequestsPerHour = InBackendSettings->GetAuthenticatedRequestsPerHour();

		// Read invite abuse config from BackendSettings (already parsed)
		const int32 InviteAbuseMaxAttempts = InBackendSettings->GetInviteAbuseMaxAttempts();
		const int32 InviteAbuseWindowSeconds = InBackendSettings->GetInviteAbuseWindowSeconds();
		const int32 InviteAbuseBanDurationSeconds = InBackendSettings->GetInviteAbuseBanDurationSeconds();

		// Read invite hourly rate limits from BackendSettings (already parsed)
		const int32 InviteCreateLimitPerHour = InBackendSettings->GetInviteCreateLimitPerHour();
		const int32 InviteUseLimitPerHour = InBackendSettings->GetInviteUseLimitPerHour();

		RateLimiter = std::make_unique<FRateLimiter>(
			RateLimitTimeToClearInMinsField.GetValueAsInt(),
			RateLimitNumberPerIPField.GetValueAsInt(),
			RateLimitPasswordField.GetValueAsInt(),
			RateLimitCreateServerField.GetValueAsInt(),
			UnauthenticatedRequestsPerHour,
			AuthenticatedRequestsPerHour,
			InviteAbuseMaxAttempts,
			InviteAbuseWindowSeconds,
			InviteAbuseBanDurationSeconds,
			InviteCreateLimitPerHour,
			InviteUseLimitPerHour
		);
	}
	else
	{
		RateLimiter = std::make_unique<FRateLimiter>(60, 10, 8, 10, 300, 2000, 10, 120, 3600, 20, 30);

		LOG_ERROR("FAbuseProtection missing BackendSettingsIni");
	}
}

bool FAbuseProtection::IsAddressBlocked(const std::string_view InAddress) const
{
	return RateLimiter->IsAddressBlocked(InAddress);
}

void FAbuseProtection::AddRateLimitedAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddProtectedActionAttempt(InAddress);
}

bool FAbuseProtection::CanAddressRequestPasswordReset(const std::string_view InAddress) const
{
	return !RateLimiter->IsPasswordResetAddressBlocked(InAddress);
}

void FAbuseProtection::AddPasswordResetAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddPasswordResetAttempt(InAddress);
}

bool FAbuseProtection::CanAddressRequestCreateServer(const std::string_view InAddress) const
{
	return !RateLimiter->IsServerOperationAddressBlocked(InAddress);
}

void FAbuseProtection::AddCreateServerAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddServerOperationAttempt(InAddress);
}

// --- Two-tier global rate limiting ---

bool FAbuseProtection::IsUnauthenticatedIPBlocked(const std::string_view InAddress) const
{
	return RateLimiter->IsUnauthenticatedIPBlocked(InAddress);
}

void FAbuseProtection::AddUnauthenticatedIPAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddUnauthenticatedIPAttempt(InAddress);
}

bool FAbuseProtection::IsAuthenticatedUserBlocked(const std::string_view UserIdStr) const
{
	return RateLimiter->IsAuthenticatedUserBlocked(UserIdStr);
}

void FAbuseProtection::AddAuthenticatedUserAttempt(const std::string_view UserIdStr) const
{
	RateLimiter->AddAuthenticatedUserAttempt(UserIdStr);
}

// --- Invite Abuse Protection ---

Uint64 FAbuseProtection::IsInviteAbuseBanned(const std::string& Ip) const
{
	return RateLimiter->IsInviteAbuseBanned(Ip);
}

bool FAbuseProtection::AddInviteAbuseAttempt(const std::string& Ip) const
{
	return RateLimiter->AddInviteAbuseAttempt(Ip);
}

void FAbuseProtection::ClearInviteAbuseForIp(const std::string& Ip) const
{
	RateLimiter->ClearInviteAbuseForIp(Ip);
}

void FAbuseProtection::PeriodicInviteAbuseCleanup() const
{
	RateLimiter->PeriodicInviteAbuseCleanup();
}

// --- Invite Hourly Rate Limits ---

bool FAbuseProtection::CanAddressCreateInvite(const std::string_view InAddress) const
{
	return !RateLimiter->IsInviteCreateAddressBlocked(InAddress);
}

void FAbuseProtection::AddCreateInviteAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddInviteCreateAttempt(InAddress);
}

bool FAbuseProtection::CanAddressUseInvite(const std::string_view InAddress) const
{
	return !RateLimiter->IsInviteUseAddressBlocked(InAddress);
}

void FAbuseProtection::AddUseInviteAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddInviteUseAttempt(InAddress);
}

CUnorderedMap<std::string, std::string> FAbuseProtection::GetCORHeaders() const
{
	return CORPolicyPtr->GetCORHeaders();
}
