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

		// Read global request cap from BackendSettings (already parsed)
		const int32 GlobalRequestsPerHour = InBackendSettings->GetGlobalRequestsPerHour();

		// Read invite abuse config from BackendSettings (already parsed)
		const int32 InviteAbuseMaxAttempts = InBackendSettings->GetInviteAbuseMaxAttempts();
		const int32 InviteAbuseWindowSeconds = InBackendSettings->GetInviteAbuseWindowSeconds();
		const int32 InviteAbuseBanDurationSeconds = InBackendSettings->GetInviteAbuseBanDurationSeconds();

		RateLimiter = std::make_unique<FRateLimiter>(
			RateLimitTimeToClearInMinsField.GetValueAsInt(),
			RateLimitNumberPerIPField.GetValueAsInt(),
			RateLimitPasswordField.GetValueAsInt(),
			RateLimitCreateServerField.GetValueAsInt(),
			GlobalRequestsPerHour,
			InviteAbuseMaxAttempts,
			InviteAbuseWindowSeconds,
			InviteAbuseBanDurationSeconds
		);
	}
	else
	{
		RateLimiter = std::make_unique<FRateLimiter>(60, 10, 8, 10, 5000, 10, 120, 3600);

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

bool FAbuseProtection::IsAddressGloballyBlocked(const std::string_view InAddress) const
{
	return RateLimiter->IsAddressGloballyBlocked(InAddress);
}

void FAbuseProtection::AddGlobalRequestAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddGlobalRequestAttempt(InAddress);
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

CUnorderedMap<std::string, std::string> FAbuseProtection::GetCORHeaders() const
{
	return CORPolicyPtr->GetCORHeaders();
}
