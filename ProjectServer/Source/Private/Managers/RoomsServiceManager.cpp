// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/RoomsServiceManager.h"
#include <cpr/cpr.h>
#include "crow/json.h"
#include "Misc/EncryptionUtil.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

FRoomsServiceManager::FRoomsServiceManager()
{
    // Get service password and address from env vars
    const char* ServicePasswordPtr = getenv("SQRLL_VOICE_API_KEY");
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

bool FRoomsServiceManager::CreateRoom(const std::string& RoomName)
{
    // If either the name OR the token is empty, abort.
    if (RoomName.empty())
    {
        return false;
    }

    // 1. Build the request body in JSON format
    crow::json::wvalue JSONData;
    JSONData["RoomId"] = RoomName;
    JSONData["Token"] = CreateRoomToken(RoomName);

    // 2. Construct the target URL
    // Let's assume ServiceAddress is "http://localhost:8082"
    const std::string TargetURL = ServiceAddress + "/api/rooms/create";

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
        try
        {
            nlohmann::json JsonResponse = nlohmann::json::parse(CPRResponse.text);

            if (JsonResponse["created"].get<bool>())
            {
                LOG_INFO("Created room in Go Voice Service.");

                return true;
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR("parsing of CreateRoom failed: " << e.what());
        }
    }
    else
    {
        LOG_ERROR("Failed to create room in Go. Status: " << CPRResponse.status_code << " Msg: " << CPRResponse.text);
    }

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
    const std::string TargetURL = ServiceAddress + "/api/rooms/check";

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
        try
        {
            if (nlohmann::json JsonResponse = nlohmann::json::parse(r.text); JsonResponse["exists"].get<bool>())
            {
                return ERoomExistenceStatus::Exists;
            }
            else
            {
                return ERoomExistenceStatus::NotExists;
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR("Error json parsing in rooms service: " << e.what());
            return ERoomExistenceStatus::Unknown;
        }
    }
    else
    {
        LOG_ERROR("Rooms service API error, Status: " << r.status_code);

        return ERoomExistenceStatus::Unknown;
    }

    return ERoomExistenceStatus::Unknown;
}

std::string FRoomsServiceManager::GetRoomToken(const std::string& RoomName)
{
    std::shared_lock<std::shared_mutex> Lock(RoomNameToTokenMutex);

    auto TokenIter = RoomNameToToken.find(RoomName);
    if (TokenIter != RoomNameToToken.end())
    {
        return TokenIter->second;
    }

    return "";
}

std::string FRoomsServiceManager::CreateRoomToken(const std::string& RoomName)
{
    bool bTokenExists = false;
    std::string Token = "";

    {
        // Lock shared to check if token exists already
        std::shared_lock<std::shared_mutex> Lock(RoomNameToTokenMutex);

        auto TokenIter = RoomNameToToken.find(RoomName);
        if (TokenIter != RoomNameToToken.end() && TokenIter->second != "")
        {
            bTokenExists = true;
            Token = TokenIter->second;
        }
    }

    if (!bTokenExists)
    {
        // @TODO: What should be actual length of Token?
        // Create token
        Token = FEncryptionUtil::GenerateSecureSalt(48);
        Token = FEncryptionUtil::ToBaseN_Irreversible(Token, FPredefinedCharsets::BASE62);

        std::unique_lock<std::shared_mutex> Lock(RoomNameToTokenMutex);
        RoomNameToToken[RoomName] = Token;
    }

    return Token;
}
