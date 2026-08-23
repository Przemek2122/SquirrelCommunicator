// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Logger/Logger.h"
#include "Managers/ImageServiceManager.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

FImageServiceManager::FImageServiceManager()
{
	const char* MasterKeyPtr = getenv("SQRLL_IMAGE_API_KEY");
	const char* ServiceAddressPtr = getenv("SQRLL_IMAGE_ADDRESS");
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

	if (MasterApiKey.empty())
	{
		LOG_WARN("SQRLL_IMAGE_API_KEY is not set - image uploads and GIFs will be unavailable.");
	}

	if (ServiceAddress.empty())
	{
		LOG_WARN("SQRLL_IMAGE_ADDRESS/SQRLL_IMAGE_PORT are not set - image service unavailable.");
	}
}

std::string FImageServiceManager::RegisterKey(const std::string& SessionToken)
{
	if (!IsEnabled() || SessionToken.empty())
	{
		return "";
	}

	// Fast path: return an existing key without hitting the service.
	{
		std::shared_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

		const auto Iter = SessionTokenToImageKey.find(SessionToken);
		if (Iter != SessionTokenToImageKey.end() && !Iter->second.empty())
		{
			return Iter->second;
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
		LOG_ERROR("Failed to register image API key. Status: " << Response.status_code << " Msg: " << Response.text);
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
		return "";
	}

	if (Key.empty())
	{
		LOG_ERROR("Image service returned an empty API key.");
		return "";
	}

	{
		std::unique_lock<std::shared_mutex> Lock(SessionTokenToImageKeyMutex);

		const auto Iter = SessionTokenToImageKey.find(SessionToken);
		if (Iter == SessionTokenToImageKey.end())
		{
			SessionTokenToImageKey[SessionToken] = Key;
		}
		else
		{
			Key = Iter->second; // Another registration won the race.
		}
	}

	LOG_DEBUG("Registered image API key for session.");
	return Key;
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
		return Iter->second;
	}

	return "";
}

bool FImageServiceManager::IsEnabled() const
{
	return !MasterApiKey.empty() && !ServiceAddress.empty();
}
