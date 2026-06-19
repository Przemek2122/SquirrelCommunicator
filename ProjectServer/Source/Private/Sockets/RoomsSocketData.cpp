// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Sockets/RoomsSocketData.h"

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Managers/RoomsServiceManager.h"
#include "SQRLLEncryption.h"
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

        case ESocketMessageRoomsType::JoinRoom:
        {


            break;
        }

        case ESocketMessageRoomsType::LeaveRoom:
        {

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
    if (RoomName.empty())
    {
        FSocket::EarlyExit(wsVariant, "empty room_name", opCode);

        return;
    }

    std::visit([&](auto&& ws) {
        std::string_view ClientIP = ws->getRemoteAddressAsText();

        FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();

        if (!ClientIP.empty() && AbuseProtection->CanAddressRequestCreateRoom(ClientIP))
        {




            AbuseProtection->AddCreateRoomAttempt(ClientIP);
        }
        else
        {
            FSocket::EarlyExit(wsVariant, "service abuse", opCode);
        }
    }, wsVariant);
}
