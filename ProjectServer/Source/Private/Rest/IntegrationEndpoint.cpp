#include "Rest/IntegrationEndpoint.h"
#include "PredefinedMessages.h"
#include "Rest/CrowUtils.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Auth/UserManager.h"

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

				// Verify google token
				try
				{
					auto response = cpr::Get(
						cpr::Url{ "https://oauth2.googleapis.com/tokeninfo" },
						cpr::Parameters{ {"id_token", GoogleToken} }
					);

					if (response.status_code == 200)
					{
						LOG_INFO("Google integration success: " << response.text);

						try
						{
							auto json = nlohmann::json::parse(response.text);

							// Verify
							if (json.contains("error"))
							{
								// Token invalid
								std::string error = json["error"];
								return false;
							}

							bool emailVerified = json["email_verified"];
							if (emailVerified)
							{
								std::string email = json["email"];
								std::string sub = json["sub"]; // Google user ID

								std::string name = json.value("name", "");
								//std::string picture = json.value("picture", "");

								// Add user (if missing)
								FUserManager* UserManager = ProjectEngine->GetUserManager();
								std::shared_ptr<FUser> UserPtr = UserManager->FindUserByMail();
								if (UserPtr != nullptr)
								{

								}
								else
								{
									// Register
									ERegisterUserStatus RegisterResult = UserManager->RegisterUser(name, "", email);
									if (RegisterResult == ERegisterUserStatus::Successful)
									{
										UserManager->FindUserByMail()
									}
								}

								if (UserPtr != nullptr)
								{
									// Create session

								}
							}
						}
						catch (const nlohmann::json::exception& e)
						{
							// Parse error
							return false;
						}
					}
					else
					{
						LOG_WARN("Google responded with error: " << response.status_code << ": " << response.text);

						ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
					}

					std::cout << "Success" << std::endl;
				}
				catch (const std::exception& e)
				{
					std::cout << "Google login exception, error: " << e.what() << std::endl;
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
