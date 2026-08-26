// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"

#include <chrono>
#include <shared_mutex>
#include <unordered_map>
#include <string>

enum class ERoomExistenceStatus
{
    Exists,
    NotExists,
    Unknown
};

/**
 * Result of a room-creation attempt against the Go Voice Service.
 * The Go service distinguishes three states:
 *   201 Created  -> brand new room
 *   200 OK       -> room already existed with the SAME token (harmless)
 *   409 Conflict -> room already existed with a DIFFERENT token (token drift)
 * Previously 200 and 409 were collapsed into a generic "Failed to create room"
 * error, which hid token drift and caused callers to silently fail. This enum
 * makes each case explicit so callers can react correctly.
 */
enum class ERoomCreateStatus
{
    Created,                     // 201 - brand new room
    AlreadyExists,               // 200 - room already existed with the SAME token
    AlreadyExistsDifferentToken, // 409 - room already existed with a DIFFERENT token (token drift)
    Failed                       // network error or unexpected status
};

/**
 * This class manages room-related microservices and provides functionality for room operations.
 *
 * @TODO: This part uses crow threads, in case of high traffic, it would be good to consider another pool of threads for voice microservice calls.
 * Now I have both services on same machine so this does not matter, but in case of high traffic or different machine it will be necessary.
 *
 * Availability: every HTTP call to the voice service is guarded by a short
 * timeout and a global circuit breaker. When the service is down (or hanging),
 * consecutive failures open the breaker and all further room checks / creates
 * short-circuit without an HTTP round-trip, so a missing voice service can
 * never stall the WebSocket event loop (voice joins are served synchronously).
 */
class FRoomsServiceManager
{
public:
    FRoomsServiceManager();

    /**
     * Call to query GO Voice Service to create a room.
     * Creates a new room with the specified name and token.
     *
     * @param RoomName The name of the room to create.
     * @return Created when the room was newly created, AlreadyExists when the
     *         room already existed with a matching token, AlreadyExistsDifferentToken
     *         when the room already existed with a different token (token drift),
     *         or Failed on any other outcome.
     */
    ERoomCreateStatus CreateRoom(const std::string& RoomName);

    /**
     * Call to query GO Voice Service to check if a room exists.
     * @param RoomName
     * @return Exists, NotExists, Unknown
     */
    ERoomExistenceStatus CheckRoom(const std::string& RoomName);

    /** @return Room token if exists, empty string otherwise */
    std::string GetRoomToken(const std::string& RoomName);

    /**
     * Configure the global circuit breaker that fails fast when the voice
     * service is unreachable.
     *
     * @param InThreshold       Number of consecutive failed HTTP calls before
     *                          the breaker opens and all further room checks /
     *                          creates short-circuit (return Unknown / Failed)
     *                          without blocking on a doomed request.
     *                          Values <= 0 disable the breaker.
     * @param InCooldownSeconds How long the breaker stays open before it
     *                          half-opens to allow a single probe to test
     *                          whether the service is back. Values <= 0 mean
     *                          the breaker, once tripped, stays open until the
     *                          backend is restarted.
     */
    void SetCircuitBreakerSettings(const int32 InThreshold, const int32 InCooldownSeconds);

    /** @return true when both the service password and the address are configured. */
    bool IsEnabled() const;

    /** @Note: connection is done by users browser. */

private:
    /** Creates token for given room. It's like password for room access */
    std::string CreateRoomToken(const std::string& RoomName);

    static std::string GenerateRandomBase64(const size_t OutLength);

private:
    std::string ServicePassword;
    std::string ServiceAddress;

    /** Mutex for RoomNameToToken */
    std::shared_mutex RoomNameToTokenMutex;

    /** Each room token */
    std::unordered_map<std::string, std::string> RoomNameToToken;

    /** Mutex guarding the circuit-breaker state below. */
    std::shared_mutex VoiceCircuitBreakerMutex;

    /** Consecutive failed voice-service HTTP calls (drives the circuit breaker). */
    int32 ConsecutiveVoiceServiceFailures = 0;

    /** Number of consecutive failures that opens the breaker. <= 0 = disabled. */
    int32 CircuitBreakerThreshold = 0;

    /** How long the breaker stays open before half-opening for a single retry. */
    std::chrono::seconds CircuitBreakerCooldown = std::chrono::seconds(60);

    /** When the breaker last opened (epoch = closed). */
    std::chrono::steady_clock::time_point CircuitOpenedAt = std::chrono::steady_clock::time_point{};

    /**
     * @return true when the circuit breaker is currently open (fail fast, no
     *         HTTP round-trip). Caller must hold VoiceCircuitBreakerMutex.
     */
    bool IsCircuitOpenLocked(std::chrono::steady_clock::time_point Now) const;

    /** Record one failed HTTP call; opens the breaker at the threshold. Caller holds lock. */
    void RecordVoiceServiceFailureLocked();

    /** Record a successful HTTP call; closes the breaker. Caller holds lock. */
    void RecordVoiceServiceSuccessLocked();
};
