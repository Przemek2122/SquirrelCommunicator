// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Sockets/RoomsSocketData.h"

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Managers/RoomsServiceManager.h"
#include "Misc/EncryptionUtil.h"
#include "Sockets/Socket.h"
#include "nlohmann/json.hpp"

FRoomsSocketData::FRoomsSocketData(FSocket* InSocket)
    : Socket(InSocket)
    , ProjectEngine(InSocket->GetProjectEngine())
{
    EncryptionKey = FEncryptionUtil::GenerateSecureSalt(64);
}

void FRoomsSocketData::PrimarySwitch(AnyWebSocket wsVariant, nlohmann::json& JsonMessage, const uWS::OpCode opCode)
{
    if (!JsonMessage.contains("type"))
    {
#if DEBUG
        LOG_ERROR("Message does not contain type");
#endif

        FSocket::EarlyExit(wsVariant, "missing type", opCode);

        // Handle error
        return;
    }

    if (!JsonMessage.contains("data"))
    {
#if DEBUG
        LOG_ERROR("Message does not contain data");
#endif

        FSocket::EarlyExit(wsVariant, "missing data", opCode);

        // Handle error
        return;
    }

    const std::string& SocketMessageRoomsType = JsonMessage["type"];
    const nlohmann::basic_json<>& DataJSON = JsonMessage["data"];
    const ESocketMessageRoomsType Type = StringToSocketMessageRoomsType(SocketMessageRoomsType);

    switch (Type)
    {
        case ESocketMessageRoomsType::CreateRoom:
        {
            if (DataJSON.contains("room_name"))
            {
                CreateRoom(wsVariant, opCode, DataJSON["room_name"]);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_name", opCode);
            }

            break;
        }

        case ESocketMessageRoomsType::Unknown:
        case ESocketMessageRoomsType::Error:
        default:
        {
            // Send error
            nlohmann::json ErrorJson;
            ErrorJson["type"] = SocketMessageRoomsTypeToString(ESocketMessageRoomsType::Error);
            ErrorJson["message"] = "Unknown message type";

            std::visit([&](auto* ws)
            {
                // Send data
                ws->send(ErrorJson.dump(), opCode);
            }, wsVariant);

            break;
        }
    }
}

void FRoomsSocketData::CreateRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& RoomName)
{
    // @TODO For now it is okay to just use Socket threads but in future we should use a thread pool for room service operations

    // 1. Basic validation
    if (RoomName.empty())
    {
        FSocket::EarlyExit(wsVariant, "empty room_name", opCode);

        return;
    }

    std::string_view ClientIP;

    std::visit([&](auto&& ws) {
        ClientIP = ws->getRemoteAddressAsText();
    }, wsVariant);

    FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();
    FRoomsServiceManager* RoomsManager = ProjectEngine->GetRoomsManager();

    if (!ClientIP.empty() && AbuseProtection->CanAddressRequestCreateRoom(ClientIP))
    {
        const std::string EncryptionTokenRandom = FEncryptionUtil::GenerateSecureSalt(32);
        const std::string RoomNameFull = RoomName + "_" + EncryptionTokenRandom;
        const std::string EncryptionToken = FEncryptionUtil::EncryptDataCustom(RoomNameFull, EncryptionKey);
        const std::string GeneratedToken = "tkn_" + EncryptionToken;


        AbuseProtection->AddCreateRoomAttempt(ClientIP);
        const bool bSuccess = RoomsManager->CreateRoom(RoomName, GeneratedToken);

        nlohmann::json response;
        response["section"] = "rooms";

        if (bSuccess)
        {
            response["type"] = "create_room_success";
            response["data"] = {
                {"room_name", RoomName},
                {"room_token", GeneratedToken}
            };
        }
        else
        {
            response["type"] = "error";
            response["data"] = {
                {"message", "Failed to create room in backend service"}
            };
        }

        // 5. Send back to client
        std::string jsonString = response.dump();
        std::visit([&](auto&& ws) {
            if (ws) ws->send(jsonString, opCode);
        }, wsVariant);
    }
    else
    {
        FSocket::EarlyExit(wsVariant, "service abuse", opCode);
    }
}
