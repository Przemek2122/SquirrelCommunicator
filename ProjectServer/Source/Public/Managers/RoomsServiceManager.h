// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once
#include <shared_mutex>

enum class ERoomExistenceStatus
{
    Exists,
    NotExists,
    Unknown
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
     * @return True if the room creation was successful, false otherwise.
     */
    bool CreateRoom(const std::string& RoomName);

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
