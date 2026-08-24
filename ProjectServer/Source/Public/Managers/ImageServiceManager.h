// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"

#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>

/**
 * Manages communication with the SquirrelCommunicatorImage (Go) microservice.
 *
 * The backend never exposes its master/admin key (SQRLL_IMAGE_API_KEY). Instead,
 * on login it asks the image service to issue a per-session API key and hands
 * that key to the user over the WebSocket ("image_api_key" event). On logout /
 * session expiry the key is revoked again, so a leaked key stops working as soon
 * as the session ends.
 *
 * Keys are kept in memory only (like the Go service's own key registry) and are
 * re-issued on the next login after a restart.
 */
class FImageServiceManager
{
public:
	FImageServiceManager();

	/**
	 * Register a new API key for the given session with the image service and
	 * store it for later delivery to the client.
	 *
	 * @return the issued key, or an empty string if the service is unavailable
	 *         or misconfigured. Idempotent: returns the existing key if one is
	 *         already registered for the session.
	 */
	std::string RegisterKey(const std::string& SessionToken);

	/** Revoke the API key associated with a session (idempotent, best effort). */
	void RevokeKey(const std::string& SessionToken);

	/** @return the stored API key for the session, or an empty string if none. */
	std::string GetKeyForSession(const std::string& SessionToken) const;

	/** @return true when both the master key and the service address are configured. */
	bool IsEnabled() const;

private:
	/** Master/admin key used to authenticate the key-management endpoints. */
	std::string MasterApiKey;

	/** Base URL of the image service (e.g. "http://localhost:8083"). */
	std::string ServiceAddress;

	/** Guards SessionTokenToImageKey and FailedKeyRegistrations. */
	mutable std::shared_mutex SessionTokenToImageKeyMutex;

	/** Per-session issued API keys. */
	std::unordered_map<std::string, std::string> SessionTokenToImageKey;

	/**
	 * Cooldown applied after a failed key registration before the same session
	 * is retried. Prevents a down/hanging image service from stalling the
	 * socket event loop with a synchronous HTTP call on every reconnect.
	 */
	static constexpr std::chrono::seconds KeyRegistrationRetryCooldown = std::chrono::seconds(30);

	/** Session tokens whose key registration recently failed, mapped to failure time. */
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> FailedKeyRegistrations;

	/** Record a failed registration timestamp (thread-safe). */
	void RecordKeyRegistrationFailure(const std::string& SessionToken);
};
