// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

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
     * @param RoomToken Token is like a password for rooms.
     * @return True if the room creation was successful, false otherwise.
     */
    bool CreateRoom(const std::string& RoomName, const std::string& RoomToken);

    /**
     * Call to query GO Voice Service to check if a room exists.
     * @param RoomName
     * @return Exists, NotExists, Unknown
     */
    ERoomExistenceStatus CheckRoom(const std::string& RoomName);

    /** @Note: connection is done by users browser. */

private:
    std::string ServicePassword;
    std::string ServiceAddress;
};
