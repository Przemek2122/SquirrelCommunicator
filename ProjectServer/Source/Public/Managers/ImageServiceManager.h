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
 *
 * Since the Go service's key registry is itself RAM-only, a restart of the image
 * service silently invalidates every key the backend has already handed out.
 * To self-heal that, each cached key carries an issue timestamp and is treated
 * as expired once its invalidation interval (configurable via the INI) elapses;
 * the next login / reconnect then re-issues a fresh key.
 *
 * Availability: every HTTP call to the image service is guarded by a short
 * timeout and a global circuit breaker. When the service is down (or hanging),
 * consecutive failures open the breaker and all further key registration /
 * instance probes short-circuit without an HTTP round-trip, so a missing image
 * service can never stall the WebSocket event loop or the login threads.
 */
class FImageServiceManager
{
public:
	FImageServiceManager();

	/**
	 * Configure the per-session key invalidation interval.
	 *
	 * @param InSeconds  Lifetime of a cached key before it is re-issued.
	 *                   Values <= 0 disable invalidation entirely (keys are
	 *                   cached indefinitely, preserving the pre-TTL behaviour).
	 */
	void SetKeyInvalidationSeconds(const int32 InSeconds);

	/**
	 * Configure how often the backend polls the image service /instance
	 * endpoint to read its "instanceId" and detect a restart.
	 *
	 * @param InSeconds  Polling interval. Values <= 0 disable the probe
	 *                   entirely (time-based invalidation set via
	 *                   SetKeyInvalidationSeconds still applies).
	 */
	void SetInstanceProbeIntervalSeconds(const int32 InSeconds);

	/**
	 * Configure the global circuit breaker that fails fast when the image
	 * service is unreachable.
	 *
	 * @param InThreshold       Number of consecutive failed HTTP calls before
	 *                          the breaker opens and all further key
	 *                          registration / instance probes short-circuit
	 *                          (return empty / skip the HTTP round-trip)
	 *                          without blocking on a doomed request.
	 *                          Values <= 0 disable the breaker.
	 * @param InCooldownSeconds How long the breaker stays open before it
	 *                          half-opens to allow a single probe to test
	 *                          whether the service is back. Values <= 0 mean
	 *                          the breaker, once tripped, stays open until the
	 *                          backend is restarted.
	 */
	void SetCircuitBreakerSettings(const int32 InThreshold, const int32 InCooldownSeconds);

	/**
	 * Poll the image service /instance endpoint (subject to the configured
	 * probe interval) and invalidate all cached per-session keys when the
	 * reported "instanceId" changes, which indicates the service restarted
	 * and lost its RAM-only key registry. Safe to call every tick; it no-ops
	 * between probes.
	 */
	void ProbeInstanceId();

	/**
	 * Register a new API key for the given session with the image service and
	 * store it for later delivery to the client.
	 *
	 * @return the issued key, or an empty string if the service is unavailable
	 *         or misconfigured. Idempotent: returns the existing key if one is
	 *         already registered for the session and still within its validity
	 *         window.
	 */
	std::string RegisterKey(const std::string& SessionToken);

	/** Revoke the API key associated with a session (idempotent, best effort). */
	void RevokeKey(const std::string& SessionToken);

	/** @return the stored API key for the session, or an empty string if none or expired. */
	std::string GetKeyForSession(const std::string& SessionToken) const;

	/** @return true when both the master key and the service address are configured. */
	bool IsEnabled() const;

private:
	/** Master/admin key used to authenticate the key-management endpoints. */
	std::string MasterApiKey;

	/** Base URL of the image service (e.g. "http://localhost:8083"). */
	std::string ServiceAddress;

	/** Guards the key map, issue-timestamp map, invalidation interval and failure map. */
	mutable std::shared_mutex SessionTokenToImageKeyMutex;

	/** Per-session issued API keys. */
	std::unordered_map<std::string, std::string> SessionTokenToImageKey;

	/** Issue timestamp for each per-session key (used for TTL invalidation). */
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> SessionTokenToImageKeyIssuedAt;

	/** Invalidation interval for cached keys. 0 = disabled (keys never expire). */
	std::chrono::seconds KeyInvalidationInterval = std::chrono::seconds(0);

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

	/**
	 * @return true when the cached key for SessionToken is still within its
	 *         invalidation window (or invalidation is disabled). Caller must
	 *         hold SessionTokenToImageKeyMutex.
	 */
	bool IsKeyStillValidLocked(const std::string& SessionToken) const;

	/**
	 * Drop every cached key (issued keys, issue timestamps and the negative
	 * cache). Called when the image service's instance id changes. Caller
	 * must hold SessionTokenToImageKeyMutex.
	 */
	void InvalidateAllCachedKeysLocked();

	/** How often to poll the image service /instance for its instance id. 0 = off. */
	std::chrono::seconds InstanceProbeInterval = std::chrono::seconds(0);

	/** When the last instance-id probe was attempted (epoch = probe immediately). */
	std::chrono::steady_clock::time_point LastInstanceProbeTime = std::chrono::steady_clock::time_point{};

	/** The last instance id reported by the image service (empty = unknown). */
	std::string LastKnownInstanceId;

	/** Consecutive failed image-service HTTP calls (drives the circuit breaker). */
	int32 ConsecutiveImageServiceFailures = 0;

	/** Number of consecutive failures that opens the breaker. <= 0 = disabled. */
	int32 CircuitBreakerThreshold = 0;

	/** How long the breaker stays open before half-opening for a single retry. */
	std::chrono::seconds CircuitBreakerCooldown = std::chrono::seconds(60);

	/** When the breaker last opened (epoch = closed). */
	std::chrono::steady_clock::time_point CircuitOpenedAt = std::chrono::steady_clock::time_point{};

	/**
	 * @return true when the circuit breaker is currently open (fail fast, no
	 *         HTTP round-trip). Caller must hold SessionTokenToImageKeyMutex.
	 */
	bool IsCircuitOpenLocked(std::chrono::steady_clock::time_point Now) const;

	/** Record one failed HTTP call; opens the breaker at the threshold. Caller holds lock. */
	void RecordImageServiceFailureLocked();

	/** Record a successful HTTP call; closes the breaker. Caller holds lock. */
	void RecordImageServiceSuccessLocked();
};
