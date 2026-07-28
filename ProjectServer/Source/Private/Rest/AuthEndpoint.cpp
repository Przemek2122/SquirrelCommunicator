#include "Rest/AuthEndpoint.h"
#include "Rest/CrowUtils.h"
#include "PredefinedMessages.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "WebUtils/CookieHelper.h"

FAuthEndpoint::FAuthEndpoint(FProjectEngine* InProjectEngine)
	: FCrowAppEndpoint(InProjectEngine)
{
}

void FAuthEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
	CROW_ROUTE(App, "/api/v1/users/register")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = FCrowUtils::CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["username"].s();
				const std::string UserPassword = JsonData["password"].s();
				const std::string EMail = JsonData["email"].s();

				const ERegisterUserStatus RegisterStatus = ProjectEngine->GetUserManager()->RegisterUser(UserName, UserPassword, EMail);

				switch (RegisterStatus)
				{
					case ERegisterUserStatus::Unknown:
					{
						OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

						break;
					}
					case ERegisterUserStatus::Successful:
					{
						ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);

						OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User registered successfully."} });

						break;
					}
					case ERegisterUserStatus::MailTaken:
					{
						OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

						break;
					}
					case ERegisterUserStatus::PasswordLengthIncorrect:
					{
						OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. Password too weak."} });

						break;
					}
					case ERegisterUserStatus::PasswordIncorrect:
					{
						OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. Password bad, please change."} });

						break;
					}
					case ERegisterUserStatus::DataBaseInsertFailed:
					{
						LOG_ERROR("ERegisterUserStatus::DataBaseInsertFailed:");

						OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

						break;
					}
					case ERegisterUserStatus::DataBaseConnectionFailed:
					{
						LOG_ERROR("ERegisterUserStatus::DataBaseConnectionFailed:");

						OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

						break;
					}
					default:
					{
						LOG_ERROR("RegisterStatus unknown case");
					}
				}
			}
			else
			{
				OutResponse = FCrowUtils::CreateResponse(429, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Too many requests."} });
			}

			return OutResponse;
		});

	CROW_ROUTE(App, "/api/v1/users/login")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserEmail = JsonData["email"].s();
				const std::string UserPassword = JsonData["password"].s();

				std::string OutSessionToken;
				const ELoginStatus LoginStatus = ProjectEngine->GetUserManager()->LoginUser(UserEmail, UserPassword, OutSessionToken);

				if (!OutSessionToken.empty() && LoginStatus == ELoginStatus::Successful)
				{
					OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User login successful!"} });

					// This is overrided by CreateResponse so use after setting OutResponse
					ProjectEngine->AddCookies(OutResponse, OutSessionToken);
				}
				else
				{
					switch (LoginStatus)
					{
						case ELoginStatus::Unknown:
						{
							OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "User login successful!"}, { "message", "unknown issue" } });

							break;
						}
						case ELoginStatus::SessionAlreadyExist:
						{
							OutResponse = FCrowUtils::CreateResponse(crow::status::NO_CONTENT, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Session already exists!"} });

							break;
						}
						case ELoginStatus::IncorrectCredentialsOrUserDoesNotExist:
						{
							OutResponse = FCrowUtils::CreateResponse(crow::status::FORBIDDEN, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Wrong credentials!"} });

							ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);

							break;
						}
						case ELoginStatus::IncorrectInputLength:
						{
							OutResponse = FCrowUtils::CreateResponse(crow::status::FORBIDDEN, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "IncorrectInputLength"} });

							ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
						}
						default:
						{
							OutResponse = FCrowUtils::CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Login: INTERNAL_SERVER_ERROR"} });
							LOG_ERROR("LoginStatus unknown value!");
						}
					}

				}
			}

			return OutResponse;
		});

	CROW_ROUTE(App, "/api/v1/users/verify")
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
				OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token correct."} });
			}
			else
			{
				OutResponse = FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token incorrect."} });

				ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
			}

			return OutResponse;
		});

	CROW_ROUTE(App, "/api/v1/users/refresh")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = FCrowUtils::CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const std::string_view CookieHeader = req.get_header_value("Cookie");
			const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

			if (!Token.empty())
			{
				if (ProjectEngine->GetUserManager()->RefreshSessionToken(Token))
				{
					OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token has new refreshed."} });
				}
				else
				{
					OutResponse = FCrowUtils::CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token not found."} });
				}

				// Get IP address
				const std::string& ClientIP = req.remote_ip_address;

				ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(ClientIP);
			}

			return OutResponse;
		});

	CROW_ROUTE(App, "/api/v1/users/logout")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string SessionToken = JsonData["token"].s();

				const bool bSuccessfullyLoggedOut = ProjectEngine->GetUserManager()->Logout(SessionToken);
				if (bSuccessfullyLoggedOut)
				{
					OutResponse = FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Session terminated!"} });
				}
				else
				{
					OutResponse = FCrowUtils::CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Can not log out."} });
				}
			}

			return OutResponse;
		});

	FCrowAppEndpoint::RegisterRoutes(App);
}
