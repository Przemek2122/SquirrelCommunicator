#include "Rest/IntegrationEndpoint.h"
#include "PredefinedMessages.h"
#include "Rest/CrowUtils.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

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

				cpr::Response response;

				// Verify google token
				try
				{
					response = cpr::Get(
						cpr::Url{"https://oauth2.googleapis.com/tokeninfo"},
						cpr::Parameters{{"id_token", GoogleToken}}
					);
				}
				catch (const std::exception& e)
				{
					std::string ExceptionText = e.what();
					LOG_ERROR("Google login exception, error: " << ExceptionText);

					OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
						{ { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
							{ "message", "Google integration - cpr: " + ExceptionText} }
					);
				}

				if (response.status_code == 200)
				{
					//LOG_INFO("Google integration success: " << response.text);

					try
					{
						auto json = nlohmann::json::parse(response.text);

						// Verify
						if (json.contains("error"))
						{
							// Token invalid
							std::string JSON_Error = json["error"];

							OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
								{ { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
									{ "message", "Google integration - JSON Contains error:" + JSON_Error } }
							);
						}

						std::string EMailVerified = json["email_verified"];
						if (EMailVerified == "true")
						{
							std::string Email = json["email"];
							std::string Name = json.value("name", "");
							//std::string picture = json.value("picture", "");

							// Add user (if missing)
							FUserManager* UserManager = ProjectEngine->GetUserManager();
							std::shared_ptr<FUser> UserPtr = UserManager->FindUserByMail(Email);
							if (UserPtr == nullptr)
							{
								// Register
								ERegisterUserStatus RegisterResult = UserManager->RegisterIntegration(Name, Email);
								if (RegisterResult == ERegisterUserStatus::Successful)
								{
									UserPtr = UserManager->FindUserByMail(Email);
								}
								else
								{
									LOG_ERROR("Google integration - Unable to register in internal database.");

									OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR, 
										{ { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
											{ "message", "Google integration - Unable to register in internal database."} }
									);
								}
							}

							if (UserPtr != nullptr)
							{
								// Create session
								std::string OutToken;
								UserManager->LoginIntegration(Email, OutToken);
							}
						}
						else
						{
							LOG_ERROR("Google integration - E-Mail not verified.");

							OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, 
								{ { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
									{ "message", "Google integration - E-Mail not verified."} }
							);
						}
					}
					catch (const nlohmann::json::exception& e)
					{
						// Parse error
						std::string ExceptionText = e.what();
						LOG_ERROR("Google integration - Parse error." << ExceptionText);

						OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR,
							{ { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
							{ "message", "Google integration - exception." + ExceptionText} }
						);
					}
				}
				else
				{
					LOG_WARN("Google responded with error: " << response.status_code << ": " << response.text);

					ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
				}

				std::cout << "Success" << std::endl;
			}
			else
			{
				ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
			}

			return OutResponse;
		});

	FCrowAppEndpoint::RegisterRoutes(App);
}
