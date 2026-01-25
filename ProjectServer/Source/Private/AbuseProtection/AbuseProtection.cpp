#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Assets/IniReader/IniObject.h"

FAbuseProtection::FAbuseProtection(FBackendSettings* InBackendSettings)
	: CORPolicyPtr(std::make_unique<FCORPolicy>())
{
	std::shared_ptr<FIniObject> BackendSettingsIni = InBackendSettings->GetBackendSettingsIni();
	if (BackendSettingsIni->DoesIniExist())
	{
		LOG_DEBUG("BackendSettingsIni number of fields: '" << BackendSettingsIni->GetNumberOfFields() << "'.");

		FIniField RateLimitTimeToClearInMinsField = BackendSettingsIni->FindFieldByName("RateLimitTimeToClearInMins");
		FIniField RateLimitNumberPerIPField = BackendSettingsIni->FindFieldByName("RateLimitNumberPerIP");
		FIniField RateLimitPasswordField = BackendSettingsIni->FindFieldByName("PasswordRateLimitNumberPerIP");

		RateLimiter = std::make_unique<FRateLimiter>(
			RateLimitTimeToClearInMinsField.GetValueAsInt(),
			RateLimitNumberPerIPField.GetValueAsInt(),
			RateLimitPasswordField.GetValueAsInt()
		);
	}
	else
	{
		RateLimiter = std::make_unique<FRateLimiter>(60, 10, 8);

		LOG_ERROR("FAbuseProtection missing BackendSettingsIni");
	}
}

bool FAbuseProtection::IsAddressBlocked(const std::string& InAddress) const
{
	return RateLimiter->IsAddressBlocked(InAddress);
}

void FAbuseProtection::AddRateLimitedAttempt(const std::string& InAddress) const
{
	RateLimiter->AddProtectedActionAttempt(InAddress);
}

bool FAbuseProtection::CanAddressRequestPasswordReset(const std::string& InAddress) const
{
	return !RateLimiter->IsPasswordResetAddressBlocked(InAddress);
}

void FAbuseProtection::AddPasswordResetAttempt(const std::string& InAddress) const
{
	RateLimiter->AddPasswordResetAttempt(InAddress);
}

CUnorderedMap<std::string, std::string> FAbuseProtection::GetCORHeaders() const
{
	return CORPolicyPtr->GetCORHeaders();
}
