#include "Rest/AccountEndpoint.h"

#include "PredefinedMessages.h"
#include "ProjectEngine.h"
#include "Auth/UserManager.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "Rest/CrowUtils.h"

FAccountEndpoint::FAccountEndpoint(FProjectEngine* InProjectEngine)
    : FCrowAppEndpoint(InProjectEngine)
{
}

void FAccountEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
    CROW_ROUTE(App, "/api/v1/account/change_name")
    .methods("POST"_method, "OPTIONS"_method)
    ([this](const crow::request& req)
    {
        crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

        const std::string_view CookieHeader = req.get_header_value("Cookie");
        const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

        // Get IP address
        const std::string& ClientIP = req.remote_ip_address;

        FUserManager* UserManager = ProjectEngine->GetUserManager();
        if (UserManager->VerifyToken(Token))
        {
            const Uint64 UserId = UserManager->GetIdFromToken(Token);

            try
            {
                const crow::json::rvalue JsonData = crow::json::load(req.body);
                if (JsonData)
                {
                    const std::string NewUserName = JsonData["new_name"].s();
                    UserManager->UpdateUserName(UserId, NewUserName);

                    // Successful set
                    OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User name changed."} });
                }
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_ERROR("Change name error: " << e.what());
                OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
                    { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
                        { "message", std::string("error :") + e.what()} }
                );
            }
        }
        else
        {
            // Wrong token
            OutResponse = FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid token."} });
        }

        return OutResponse;
    });

    CROW_ROUTE(App, "/api/v1/account/change_password")
     .methods("POST"_method, "OPTIONS"_method)
     ([this](const crow::request& req)
     {
         crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

         const std::string_view CookieHeader = req.get_header_value("Cookie");
         const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

         // Get IP address
         const std::string& ClientIP = req.remote_ip_address;

         FUserManager* UserManager = ProjectEngine->GetUserManager();
         if (UserManager->VerifyToken(Token))
         {
             const Uint64 UserId = UserManager->GetIdFromToken(Token);

             try
             {
                 const crow::json::rvalue JsonData = crow::json::load(req.body);
                 if (JsonData)
                 {
                     const std::string OldPassword = JsonData["old_password"].s();
                     const std::string NewPassword = JsonData["new_password"].s();
                     UserManager->UpdateUserPassword(UserId, OldPassword, NewPassword);

                     // Successful set
                     OutResponse = FCrowUtils::CreateResponse(crow::status::OK,
                         { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success },
                             { "message", "User password changed."} }
                     );
                 }
             }
             catch (const nlohmann::json::exception& e)
             {
                 LOG_ERROR("Change pass error: " << e.what());
                 OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
                     { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
                         { "message", std::string("error :") + e.what()} }
                 );
             }
         }
         else
         {
             // Wrong token
             OutResponse = FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid token."} });
         }

         return OutResponse;
     });

    FCrowAppEndpoint::RegisterRoutes(App);
}
