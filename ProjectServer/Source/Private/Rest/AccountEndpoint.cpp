#include "Rest/AccountEndpoint.h"

#include "PredefinedMessages.h"
#include "ProjectEngine.h"
#include "Auth/UserManager.h"
#include "Misc/WebSockets/CookieHelper.h"
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

        if (ProjectEngine->GetUserManager()->VerifyToken(Token))
        {





            OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "User name changed."} });
        }
        else
        {

        }


        return OutResponse;
    });

    FCrowAppEndpoint::RegisterRoutes(App);
}
