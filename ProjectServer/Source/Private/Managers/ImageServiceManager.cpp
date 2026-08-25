// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Logger/Logger.h"
#include "Managers/ImageServiceManager.h"

#include <chrono>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

FImageServiceManager::FImageServiceManager()
{
	const char* MasterKeyPtr = getenv("SQRLL_IMAGE_API_KEY");
	const char* ServiceAddressPtr = getenv("SQRLL_IMAGE_SERVICE_URL");
	const char* ServicePortPtr = getenv("SQRLL_IMAGE_PORT");

	MasterApiKey = (MasterKeyPtr != nullptr) ? MasterKeyPtr : "";
	if (ServiceAddressPtr != nullptr && ServicePortPtr != nullptr)
	{
		ServiceAddress = std::string(ServiceAddressPtr) + ":" + ServicePortPtr;
	}
	else if (ServiceAddressPtr != nullptr)
	{
		// Allow a single variable that already contains the port.
		ServiceAddress = ServiceAddressPtr;
	}

	// Normalize the base URL so it always carries an explicit scheme. The
	// config examples set SQRLL_IMAGE_SERVICE_URL to a bare host
	// ("127.0.0.1"); without a scheme libcurl's behaviour is version-dependent
	// (some builds guess http://, others reject the URL outright). The image
	// service is plain HTTP internally, so default to that.
	if (!ServiceAddress.empty())
	{
		if (ServiceAddress.rfind("http://", 0) != 0 &&
			ServiceAddress.rfind("https://", 0) != 0)
		{
			ServiceAddress = "http://" + ServiceAddress;
		}
	}

	if (MasterApiKey.empty())
	{
		LOG_WARN("SQRLL_IMAGE_API_KEY is not set - image uploads and GIFs will be unavailable.");
	}

	if (ServiceAddress.empty())
	{
		LOG_WARN("SQRLL_IMAGE_SERVICE_URL/SQRLL_IMAGE_PORT are not set - image service unavailable.");
	}
}

void FImageServiceManager::SetKeyInvalidationSeconds(const int32 InSeconds)
{
	std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

	// Values <= 0 disable invalidation entirely (keys are cached indefinitely,
	// preserving the pre-TTL behaviour).
	KeyInvalidationInterval = (InSeconds > 0) ? std::chrono::seconds(InSeconds) : std::chrono::seconds(0);

	if (KeyInvalidationInterval.count() <= 0)
	{
		// Timestamps are no longer consulted; drop them so the map doesn't grow.
		SessionTokenToImageKeyIssuedAt.clear();
	}
}

void FImageServiceManager::SetInstanceProbeIntervalSeconds(const int32 InSeconds)
{
	std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

	InstanceProbeInterval = (InSeconds > 0) ? std::chrono::seconds(InSeconds) : std::chrono::seconds(0);

	if (InstanceProbeInterval.count() <= 0)
	{
		// Probe disabled; forget any instance id we learnt so re-enabling starts
		// from a clean slate and probes immediately.
		LastKnownInstanceId.clear();
		LastInstanceProbeTime = std::chrono::steady_clock::time_point{};
	}
}

void FImageServiceManager::ProbeInstanceId()
{
	if (!IsEnabled())
	{
		return;
	}

	// Gate on the configured interval. LastInstanceProbeTime starts at the epoch
	// so the first probe fires on the first call rather than waiting a full interval.
	{
		std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);
		if (InstanceProbeInterval.count() <= 0)
		{
			return;
		}
		const auto Now = std::chrono::steady_clock::now();
		if ((Now - LastInstanceProbeTime) < InstanceProbeInterval)
		{
			return;
		}
		LastInstanceProbeTime = Now;
	}

	// Perform the HTTP request OUTSIDE the lock so a slow/hung image service can
	// never block key lookups. A short timeout bounds how long the tick thread
	// may stall.
	const std::string TargetURL = ServiceAddress + "/instance";
	const cpr::Response Response = cpr::Get(
		cpr::Url{TargetURL},
		cpr::Timeout{2000}
	);

	if (Response.status_code != 200)
	{
		return;
	}

	std::string InstanceId;
	try
	{
		const nlohmann::json JsonResponse = nlohmann::json::parse(Response.text);
		if (JsonResponse.contains("instanceId") && JsonResponse["instanceId"].is_string())
		{
			InstanceId = JsonResponse["instanceId"].get<std::string>();
		}
	}
	catch (const nlohmann::json::exception&)
	{
		return;
	}

	if (InstanceId.empty())
	{
		// Older image service without the /instance endpoint (or one that
		// returns a non-200 / empty body); fall back to the time-based (TTL)
		// invalidation only.
		return;
	}

	std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

	if (LastKnownInstanceId.empty())
	{
		// First observation: just record it. Nothing has been issued against a
		// previous instance, so there is nothing to invalidate.
		LastKnownInstanceId = InstanceId;
		LOG_DEBUG("Recorded image service instance id.");
		return;
	}

	if (LastKnownInstanceId != InstanceId)
	{
		LOG_WARN("Image service instance id changed - the service restarted and lost its RAM-only key registry. Invalidating all cached per-session keys.");
		InvalidateAllCachedKeysLocked();
		LastKnownInstanceId = InstanceId;
	}
}

void FImageServiceManager::InvalidateAllCachedKeysLocked()
{
	SessionTokenToImageKey.clear();
	SessionTokenToImageKeyIssuedAt.clear();
	FailedKeyRegistrations.clear();
}

bool FImageServiceManager::IsKeyStillValidLocked(const std::string& SessionToken) const
{
	// Invalidation disabled -> keys never expire.
	if (KeyInvalidationInterval.count() <= 0)
	{
		return true;
	}

	const auto TimeIter = SessionTokenToImageKeyIssuedAt.find(SessionToken);
	if (TimeIter == SessionTokenToImageKeyIssuedAt.end())
	{
		// No issue timestamp recorded (shouldn't happen for freshly issued keys);
		// treat as stale so it gets re-issued.
		return false;
	}

	return (std::chrono::steady_clock::now() - TimeIter->second) < KeyInvalidationInterval;
}

std::string FImageServiceManager::RegisterKey(const std::string& SessionToken)
{
	if (!IsEnabled() || SessionToken.empty())
	{
		return "";
	}

	// Fast path: return an existing key if it is still within its validity window.
	{
		std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

		const auto Iter = SessionTokenToImageKey.find(SessionToken);
		if (Iter != SessionTokenToImageKey.end() && !Iter->second.empty())
		{
			if (IsKeyStillValidLocked(SessionToken))
			{
				return Iter->second;
			}

			// The key has exceeded its invalidation interval; drop it so we
			// re-issue a fresh one below. This is what self-heals stale keys
			// after an image-service restart.
			SessionTokenToImageKey.erase(Iter);
			SessionTokenToImageKeyIssuedAt.erase(SessionToken);
		}

		// Negative cache: skip the HTTP call if a registration for this session
		// recently failed, so a down image service can't stall the socket event
		// loop on every reconnect. Retried once the cooldown elapses.
		const auto FailIter = FailedKeyRegistrations.find(SessionToken);
		if (FailIter != FailedKeyRegistrations.end() &&
			(std::chrono::steady_clock::now() - FailIter->second) < KeyRegistrationRetryCooldown)
		{
			return "";
		}
	}

	const std::string TargetURL = ServiceAddress + "/api/key";

	cpr::Response Response = cpr::Post(
		cpr::Url{TargetURL},
		cpr::Header{{"X-SQRLL-IMAGE-API-KEY", MasterApiKey}},
		cpr::Timeout{3000}
	);

	if (Response.status_code != 201)
	{
		RecordKeyRegistrationFailure(SessionToken);
		if (Response.status_code == 403)
		{
			LOG_ERROR("Image service rejected the master key (403). SQRLL_IMAGE_API_KEY mismatch between backend and image service - no per-session key will be issued and GIF/upload requests will fail with 401.");
		}
		else
		{
			LOG_ERROR("Failed to register image API key. Status: " << Response.status_code << " Msg: " << Response.text);
		}
		return "";
	}

	std::string Key;
	try
	{
		const nlohmann::json JsonResponse = nlohmann::json::parse(Response.text);
		if (JsonResponse.contains("key") && JsonResponse["key"].is_string())
		{
			Key = JsonResponse["key"].get<std::string>();
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG_ERROR("Failed to parse image API key registration response: " << e.what());
		RecordKeyRegistrationFailure(SessionToken);
		return "";
	}

	if (Key.empty())
	{
		LOG_ERROR("Image service returned an empty API key.");
		RecordKeyRegistrationFailure(SessionToken);
		return "";
	}

	{
		std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

		// Success clears any prior negative-cache entry.
		FailedKeyRegistrations.erase(SessionToken);

		const auto Iter = SessionTokenToImageKey.find(SessionToken);
		if (Iter == SessionTokenToImageKey.end())
		{
			SessionTokenToImageKey[SessionToken] = Key;
			SessionTokenToImageKeyIssuedAt[SessionToken] = std::chrono::steady_clock::now();
		}
		else
		{
			Key = Iter->second; // Another registration won the race.
		}
	}

	LOG_DEBUG("Registered image API key for session.");
	return Key;
}

void FImageServiceManager::RecordKeyRegistrationFailure(const std::string& SessionToken)
{
	std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);
	FailedKeyRegistrations[SessionToken] = std::chrono::steady_clock::now();
}

void FImageServiceManager::RevokeKey(const std::string& SessionToken)
{
	if (SessionToken.empty())
	{
		return;
	}

	std::string Key;
	{
		std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

		const auto Iter = SessionTokenToImageKey.find(SessionToken);
		if (Iter == SessionTokenToImageKey.end())
		{
			return; // Nothing to revoke.
		}

		Key = Iter->second;
		SessionTokenToImageKey.erase(Iter);
		SessionTokenToImageKeyIssuedAt.erase(SessionToken);
	}

	if (!IsEnabled() || Key.empty())
	{
		return;
	}

	const std::string TargetURL = ServiceAddress + "/api/key";

	cpr::Response Response = cpr::Delete(
		cpr::Url{TargetURL},
		cpr::Header{
			{"X-SQRLL-IMAGE-API-KEY", MasterApiKey},
			{"X-SQRLL-API-KEY", Key}
		},
		cpr::Timeout{3000}
	);

	if (Response.status_code != 200)
	{
		LOG_ERROR("Failed to revoke image API key. Status: " << Response.status_code << " Msg: " << Response.text);
	}
}

std::string FImageServiceManager::GetKeyForSession(const std::string& SessionToken) const
{
	std::shared_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

	const auto Iter = SessionTokenToImageKey.find(SessionToken);
	if (Iter != SessionTokenToImageKey.end())
	{
		// Honour the invalidation interval: an expired key is treated as absent
		// so the caller falls back to RegisterKey and re-issues it.
		if (IsKeyStillValidLocked(SessionToken))
		{
			return Iter->second;
		}
	}

	return "";
}

bool FImageServiceManager::IsEnabled() const
{
	return !MasterApiKey.empty() && !ServiceAddress.empty();
}
