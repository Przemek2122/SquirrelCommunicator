// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Rest/TransferTokenEndpoint.h"

#include <SDL3/SDL_stdinc.h>

#include "PredefinedMessages.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/TransferTokenManager.h"
#include "Auth/UserManager.h"
#include "Misc/Util.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "Rest/CrowUtils.h"

FTransferTokenEndpoint::FTransferTokenEndpoint(FProjectEngine* InProjectEngine)
    : FCrowAppEndpoint(InProjectEngine)
{
}

void FTransferTokenEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
    CROW_ROUTE(App, "/api/v1/transfer_token/create")
        .methods("POST"_method, "OPTIONS"_method)
        ([this](const crow::request& req)
        {
            crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "error."} });

            // @TODO: Add specific limiting of requests per IP address
            const std::string& ClientIP = req.remote_ip_address;
            ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);

            const std::string CookieHeader = req.get_header_value("Cookie");
            const std::string AuthToken = ProjectEngine->ExtractCookieValue(CookieHeader, "auth_token");

            // Fail fast if token is missing
            if (AuthToken.empty())
            {
                return FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED,
                    { { "status", "error" }, { "message", "Missing or invalid session."} }
                );
            }

            FUserManager* UserManager = ProjectEngine->GetUserManager();
            const Uint64 UserId = UserManager->GetIdFromToken(AuthToken);
            if (UserId > 0)
            {
                FTransferTokenManager* TransferTokenManager = ProjectEngine->GetTransferTokenManager();
                const std::string Token = TransferTokenManager->CreateTransferToken(UserId);

                if (Token.empty())
                {
                    LOG_ERROR("Failed to create transfer token for user with ID: " + std::to_string(UserId));
                    return FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
                        { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Failed to create transfer token."} });
                }

                return FCrowUtils::CreateResponse(crow::status::OK,
                    { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "token", Token } });
            }
            else
            {
                return FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED,
                    { { "status", "error" }, { "message", "Token incorrect."} }
                );
            }

            return OutResponse;
        });

    CROW_ROUTE(App, "/api/v1/transfer_token/redeem")
        .methods("POST"_method, "OPTIONS"_method)
        ([this](const crow::request& req)
        {
            crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

            std::string Token;

            try
            {
                const nlohmann::json JsonData = nlohmann::json::parse(req.body);

                Token = JsonData.at("token").get<std::string>();
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_WARN("Password reset JSON error: " << e.what());

                OutResponse = FCrowUtils::CreateResponse(
                    crow::status::BAD_REQUEST,
                    {
                        { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
                        { "message", "Invalid request format or missing required fields." }
                    }
                );
            }

            if (Token.empty())
            {
                return OutResponse;
            }

            FTransferTokenManager* TransferTokensManager = ProjectEngine->GetTransferTokenManager();
            if (TransferTokensManager->IsTransferTokenValid(Token))
            {
                const Uint64 UserId = TransferTokensManager->GetUserIdFromTransferToken(Token);
                if (UserId > 0)
                {
                    TransferTokensManager->RemoveTransferToken(Token);

                    FUserManager* UserManager = ProjectEngine->GetUserManager();
                    std::string OutSessionToken;
                    const ELoginStatus LoginResult = UserManager->LoginFromId(UserId, OutSessionToken);

                    if (LoginResult == ELoginStatus::Successful && !OutSessionToken.empty())
                    {
                        OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Transfer token redeemed."} });

                        // This is overrided by CreateResponse so use after setting OutResponse
                        ProjectEngine->AddCookies(OutResponse, OutSessionToken);
                    }
                    else
                    {
                        LOG_DEBUG("Failed to login user with ID: " << UserId << " after redeeming transfer token");
                    }
                }
                else
                {
                    LOG_DEBUG("Invalid user ID in transfer token: " + std::to_string(UserId));
                    OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid user ID."} });
                }
            }
            else
            {
                LOG_DEBUG("Invalid transfer token: " + Token);
                OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid transfer token."} });
            }

            return OutResponse;
        });

    FCrowAppEndpoint::RegisterRoutes(App);
}
