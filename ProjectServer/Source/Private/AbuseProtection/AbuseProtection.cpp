#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Assets/IniReader/IniObject.h"

FAbuseProtection::FAbuseProtection(FBackendSettings* InBackendSettings)
	: CORPolicyPtr(std::make_unique<FCORPolicy>())
{
	std::shared_ptr<FIniObject> BackendSettingsIni = InBackendSettings->GetBackendSettingsIni();
	if (BackendSettingsIni->DoesIniExist())
	{
		FIniField RateLimitNumberPerIPField = BackendSettingsIni->FindFieldByName("RateLimitNumberPerIP");
		FIniField RateLimitTimeToClearInMinsField = BackendSettingsIni->FindFieldByName("RateLimitTimeToClearInMins");

		RateLimiter = std::make_unique<FRateLimiter>(RateLimitTimeToClearInMinsField.GetValueAsInt(), RateLimitNumberPerIPField.GetValueAsInt());
	}
	else
	{
		RateLimiter = std::make_unique<FRateLimiter>(60, 10);

		LOG_ERROR("FAbuseProtection missing BackendSettingsIni");
	}
}

bool FAbuseProtection::IsAddressBlocked(const std::string& InAddress)
{
	return RateLimiter->IsAddressBlocked(InAddress);
}

void FAbuseProtection::AddRateLimitedAttempt(const std::string& InAddress)
{
	RateLimiter->AddProtectedActionAttempt(InAddress);
}

CUnorderedMap<std::string, std::string> FAbuseProtection::GetCORHeaders() const
{
	return CORPolicyPtr->GetCORHeaders();
}
