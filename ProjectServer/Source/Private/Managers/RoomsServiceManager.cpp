// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/RoomsServiceManager.h"
#include <cpr/cpr.h>
#include "crow/json.h"

FRoomsServiceManager::FRoomsServiceManager()
{
    // Get service password and address from env vars
    const char* ServicePasswordPtr = getenv("SQRLL_VOICE_PASSWORD");
    const char* ServiceAddressPtr = getenv("SQRLL_VOICE_ADDRESS");
    const char* ServicePortPtr = getenv("SQRLL_VOICE_PORT");
    ServicePassword = ServicePasswordPtr != nullptr ? ServicePasswordPtr : "";
    if (ServiceAddressPtr != nullptr && ServicePortPtr != nullptr)
    {
        ServiceAddress = ServiceAddressPtr + std::string(":") + ServicePortPtr;
    }

    if (ServicePassword.empty())
    {
        LOG_ERROR("Service password is empty!");
    }

    if (ServiceAddress.empty())
    {
        LOG_ERROR("Service address is empty!");
    }
}

bool FRoomsServiceManager::CreateRoom(const std::string& RoomName, const std::string& RoomToken)
{
    // If either the name OR the token is empty, abort.
    if (RoomName.empty() || RoomToken.empty())
    {
        return false;
    }

    // 1. Build the request body in JSON format
    crow::json::wvalue JSONData;
    JSONData["RoomId"] = RoomName;
    JSONData["Token"] = RoomToken;

    // 2. Construct the target URL
    // Let's assume ServiceAddress is "http://localhost:8082"
    const std::string TargetURL = ServiceAddress + "/api/create_room";

    // 3. Synchronous POST request (blocks the current thread, which is fine here)
    cpr::Response CPRResponse = cpr::Post(
        cpr::Url{TargetURL},
        cpr::Header{
            {"Content-Type", "application/json"},
            {"X-API-Token", ServicePassword}       // Pass the password for Go verification
        },
        cpr::Body{JSONData.dump()},               // Inject the generated JSON string
        cpr::Timeout{3000}                         // Failsafe: max 3 seconds wait time
    );

    // 4. Verify the response from Go
    // Your Go code uses w.WriteHeader(http.StatusCreated), which is code 201.
    if (CPRResponse.status_code == 201)
    {
        return true;
    }

    LOG_ERROR("Failed to create room in Go. Status: " << CPRResponse.status_code << " Msg: " << CPRResponse.text);

    return false;
}

ERoomExistenceStatus FRoomsServiceManager::CheckRoom(const std::string& RoomName)
{
    // If the room name is empty, there is nothing to check
    if (RoomName.empty())
    {
        return ERoomExistenceStatus::Unknown;
    }

    // 1. Construct the target URL base
    std::string TargetURL = ServiceAddress + "/api/check_room";

    // 2. Synchronous GET request
    cpr::Response r = cpr::Get(
        cpr::Url{TargetURL},
        cpr::Parameters{{"room", RoomName}},       // Automatically builds "?room=RoomName"
        cpr::Header{
            {"X-API-Token", ServicePassword}       // Pass the password for Go verification
        },
        cpr::Timeout{3000}                         // Failsafe: max 3 seconds wait time
    );

    // 3. Verify the response from Go
    // GO Voice Service code uses w.WriteHeader(http.StatusOK) for success, which is code 200.
    if (r.status_code == 200)
    {
        // Room exists
        return ERoomExistenceStatus::Exists;
    }

    if (r.status_code == 404)
    {
        // Room does not exist (this is an expected, valid response)
        return ERoomExistenceStatus::NotExists;
    }

    return ERoomExistenceStatus::Unknown;
}
