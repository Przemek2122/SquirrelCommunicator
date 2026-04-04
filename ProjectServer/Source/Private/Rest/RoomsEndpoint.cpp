// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Rest/RoomsEndpoint.h"
#include "PredefinedMessages.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Managers/RoomsServiceManager.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "Rest/CrowUtils.h"

class FAbuseProtection;

FRoomsEndpoint::FRoomsEndpoint(FProjectEngine* InProjectEngine)
{
}

void FRoomsEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
    CROW_ROUTE(App, "/rooms/create")
    .methods("POST"_method, "OPTIONS"_method)
    ([this](const crow::request& req)
    {
        crow::response OutResponse = FCrowUtils::CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

        // Get IP address
        const std::string& ClientIP = req.remote_ip_address;

        FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();
        if (AbuseProtection->CanAddressRequestCreateRoom(ClientIP))
        {
            std::string RoomId;
            std::string RoomToken;

            try
            {
                const crow::json::rvalue JsonData = crow::json::load(req.body);
                if (JsonData)
                {
                    RoomId = JsonData["room_id"].s();
                    RoomToken = JsonData["room_token"].s();
                }
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_ERROR("Create room error: " << e.what());
                OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
                    { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
                        { "message", std::string("error :") + e.what()} }
                );
            }

            if (RoomId.empty())
            {
                LOG_ERROR("Create room error: Room ID is empty");
                OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST,
                    { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
                        { "message", "Room ID is empty"} }
                );
                return OutResponse;
            }

            AbuseProtection->AddCreateRoomAttempt(ClientIP);
            ProjectEngine->GetRoomsManager()->CreateRoom(RoomId, RoomToken);


        }

        return OutResponse;
    });

    FCrowAppEndpoint::RegisterRoutes(App);
}

