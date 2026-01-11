#include "Rest/IntegrationEndpoint.h"
#include "PredefinedMessages.h"
#include "Rest/CrowUtils.h"
#include <cpr/cpr.h>

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"

FIntegrationEndpoint::FIntegrationEndpoint(FProjectEngine* InProjectEngine)
	: FCrowAppEndpoint(InProjectEngine)
{
}

void FIntegrationEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
	CROW_ROUTE(App, "/api/v1/integrate/google")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string GoogleToken = JsonData["google_token"].s();

				// Verify frontend
				// Make request to https://oauth2.googleapis.com/tokeninfo?id_token=TOKEN

				auto response = cpr::Get(
					cpr::Url{ "https://oauth2.googleapis.com/tokeninfo" },
					cpr::Parameters{ {"id_token", GoogleToken} }
				);

				if (response.status_code == 200)
				{
					LOG_INFO("Google integration success: " << response.text);

					// Add user (if missing)


					// Create session


				}
				else
				{
					LOG_WARN("Google responded with error: " << response.status_code << ": " << response.text);

					ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
				}
			}
			else
			{
				ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
			}

			return OutResponse;
		});

	FCrowAppEndpoint::RegisterRoutes(App);
}
