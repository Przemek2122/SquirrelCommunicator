#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Assets/IniReader/IniObject.h"

FAbuseProtection::FAbuseProtection(const FBackendSettings* InBackendSettings)
	: CORPolicyPtr(std::make_unique<FCORPolicy>())
{
	const std::shared_ptr<FIniObject> BackendSettingsIni = InBackendSettings->GetBackendSettingsIni();
	if (BackendSettingsIni->DoesIniExist())
	{
		LOG_DEBUG("BackendSettingsIni number of fields: '" << BackendSettingsIni->GetNumberOfFields() << "'.");

		const FIniField RateLimitTimeToClearInMinsField = BackendSettingsIni->FindFieldByName("RateLimitTimeToClearInMins");
		const FIniField RateLimitNumberPerIPField = BackendSettingsIni->FindFieldByName("RateLimitNumberPerIP");
		const FIniField RateLimitPasswordField = BackendSettingsIni->FindFieldByName("PasswordRateLimitNumberPerIP");
		const FIniField RateLimitCreateRoomField = BackendSettingsIni->FindFieldByName("CreateRoomRateLimitNumberPerIP");

		RateLimiter = std::make_unique<FRateLimiter>(
			RateLimitTimeToClearInMinsField.GetValueAsInt(),
			RateLimitNumberPerIPField.GetValueAsInt(),
			RateLimitPasswordField.GetValueAsInt(),
			RateLimitCreateRoomField.GetValueAsInt()
		);
	}
	else
	{
		RateLimiter = std::make_unique<FRateLimiter>(60, 10, 8, 10);

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

bool FAbuseProtection::CanAddressRequestCreateRoom(const std::string_view InAddress) const
{
	return RateLimiter->IsRoomOperationAddressBlocked(InAddress);
}

void FAbuseProtection::AddCreateRoomAttempt(const std::string_view InAddress) const
{
	RateLimiter->AddRoomOperationAttempt(InAddress);
}

CUnorderedMap<std::string, std::string> FAbuseProtection::GetCORHeaders() const
{
	return CORPolicyPtr->GetCORHeaders();
}
