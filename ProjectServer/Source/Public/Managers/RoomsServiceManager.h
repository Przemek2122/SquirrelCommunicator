// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

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
};
