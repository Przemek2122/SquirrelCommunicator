#include "ProjectEngine.h"

#include "PredefinedMessages.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Assets/IniReader/IniObject.h"
#include "DataBase/DataBaseSettings.h"
#include "Sockets/SocketManager.h"

void FCrowAppMiddleware::before_handle(crow::request& Req, crow::response& Res, context& Ctx)
{
	FProjectEngine* ProjectEngine = static_cast<FProjectEngine*>(FGlobalDefines::GEngine);
	FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();

	// Get IP address
	const std::string& ClientIP = Req.remote_ip_address;

	if (!AbuseProtection->IsAddressBlocked(ClientIP))
	{
		// Options support
		if (Req.method == crow::HTTPMethod::Options)
		{
			Res.code = 204;
			Res.end();
		}
	}
	else
	{
		// Block
		Res.code = crow::status::TOO_MANY_REQUESTS;  // Too Many Requests
		Res.body = "{\"error\":\"Rate limit exceeded\"}";
		Res.end();
	}
}

void FCrowAppMiddleware::after_handle(crow::request& Req, crow::response& Res, context& Ctx)
{
	static const std::string AccessControlAllowOriginHeaderName = "Access-Control-Allow-Origin";

	const std::string Origin = Req.get_header_value("Origin");
	FProjectEngine* ProjectEngine = static_cast<FProjectEngine*>(FGlobalDefines::GEngine);
	const CArray<std::string>& Whitelist = ProjectEngine->GetOriginWhitelist();

	ProjectEngine->AddHeaders(Res, ProjectEngine->GetDefaultHeadersCache());

	if (Whitelist.Size() && Whitelist.Contains(Origin))
	{
		ProjectEngine->AddHeaders(Res, { { AccessControlAllowOriginHeaderName, Origin } });
	}
	else
	{
		ProjectEngine->AddHeaders(Res, { { AccessControlAllowOriginHeaderName, Whitelist[0]}});
	}
}

FProjectEngine::FProjectEngine()
	: BackendSettings(std::make_unique<FBackendSettings>())
	, SocketManager(std::make_unique<FSocketManager>())
	, bIsSSLEnabled(false)
{
	// Collect Database settings
	FDataBaseSettings::Initialize();
}

void FProjectEngine::Init()
{
	/** We do not need SDL input in server */
	DisableInput();

	FEngine::Init();

	LOG_DEBUG("Server init");

	UserManager = std::make_unique<FUserManager>();
	BackendSettings->LoadBackendSettings();
	AbuseProtectionPtr = std::make_unique<FAbuseProtection>(BackendSettings.get());
	DefaultHeadersCache = GetDefaultHeaders();

	std::shared_ptr<FIniObject> ServerSettingsIni = BackendSettings->GetBackendSettingsIni();
	if (ServerSettingsIni->DoesIniExist())
	{
		OriginWhitelist.Push("http://localhost");
		const FIniField BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress");
		if (BackendAddressField.IsValid())
		{
			OriginWhitelist.Push(BackendAddressField.GetValueAsString());
		}

		InitBasicSetup();

		LOG_DEBUG("Created api test");

		InitUsersSetup();

		LOG_DEBUG("Created api user");

		UserManager->Init();

		// HTTP/REST crow server
		StartServer(ServerSettingsIni);

		// Socket
		const FIniField PortWSField = ServerSettingsIni->FindFieldByName("PortWS");
		if (PortWSField.IsValid())
		{
			//SocketManager->CreateSockets(PortWSField.GetValueAsInt(), bIsSSLEnabled, KeyFilePath, CertFilePath);
		}

		LOG_DEBUG("Created socket messages");
	}
	else
	{
		LOG_ERROR("Ini is missing, API will not work.");
	}
}

void FProjectEngine::PostSecondTick()
{
	FEngine::PostSecondTick();

	UserManager->PostSecondTick();
}

void FProjectEngine::InitBasicSetup()
{
	// Most common address to check if it works
	CROW_ROUTE(CrowApp, "/")([this]()
		{
			return CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Crow C++ API Server is running."} });
		});

	// Route for testing if api works
	CROW_ROUTE(CrowApp, "/api/v1/test")([this]()
		{
			return CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
		});
}

void FProjectEngine::InitUsersSetup()
{
	CROW_ROUTE(CrowApp, "/api/v1/users/register")
		.methods("POST"_method, "OPTIONS"_method)
		([this] (const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			if (!AbuseProtectionPtr->IsAddressBlocked(ClientIP))
			{
				const crow::json::rvalue JsonData = crow::json::load(req.body);
				if (JsonData)
				{
					const std::string UserName = JsonData["username"].s();
					const std::string UserPassword = JsonData["password"].s();
					const std::string EMail = JsonData["email"].s();

					const ERegisterUserStatus RegisterStatus = UserManager->RegisterUser(UserName, UserPassword, EMail);

					switch (RegisterStatus)
					{
						case ERegisterUserStatus::Unknown:
						{
							OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

							break;
						}
						case ERegisterUserStatus::Successful:
						{
							AbuseProtectionPtr->AddRateLimitedAttempt(ClientIP);

							OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User registered successfully."} });

							break;
						}
						case ERegisterUserStatus::MailTaken:
						{
							OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

							break;
						}
						case ERegisterUserStatus::PasswordToWeak:
						{
							OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. Password too weak."} });

							break;
						}
						case ERegisterUserStatus::DataBaseInsertFailed:
						{
							LOG_ERROR("ERegisterUserStatus::DataBaseInsertFailed:");

							OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

							break;
						}
						case ERegisterUserStatus::DataBaseConnectionFailed:
						{
							LOG_ERROR("ERegisterUserStatus::DataBaseConnectionFailed:");

							OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

							break;
						}
						default:
						{
							LOG_ERROR("RegisterStatus unknown case");
						}
					}
				}
			}
			else
			{
				OutResponse = CreateResponse(429, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Too many requests."} });
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/login")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			if (!AbuseProtectionPtr->IsAddressBlocked(ClientIP))
			{
				const crow::json::rvalue JsonData = crow::json::load(req.body);
				if (JsonData)
				{
					const std::string UserEmail = JsonData["email"].s();
					const std::string UserPassword = JsonData["password"].s();

					std::string OutSessionToken;
					const ELoginStatus LoginStatus = UserManager->LoginUser(UserEmail, UserPassword, OutSessionToken);

					if (!OutSessionToken.empty())
					{
						switch (LoginStatus)
						{
							case ELoginStatus::Unknown:
							{
								OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "User login successful!"}, { "message", "unknown issue" } });

								break;
							}
							case ELoginStatus::Successful:
							{
								OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User login successful!"} });
								AddCookies(OutResponse, OutSessionToken);

								break;
							}
							case ELoginStatus::SessionAlreadyExist:
							{
								OutResponse = CreateResponse(crow::status::NO_CONTENT, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Session already exists!"} });

								break;
							}
							case ELoginStatus::IncorrectCredentialsOrUserDoesNotExist:
							{
								OutResponse = CreateResponse(crow::status::FORBIDDEN, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Wrong credentials!"} });

								break;
							}
							default:
							{
								LOG_ERROR("LoginStatus unknown value!");
							}
						}
					}
					else
					{
						OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Unable to generate session."} });
					}
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/refresh")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
			{
				crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

				// Get IP address
				const std::string& ClientIP = req.remote_ip_address;

				if (!AbuseProtectionPtr->IsAddressBlocked(ClientIP))
				{
					const crow::json::rvalue JsonData = crow::json::load(req.body);
					if (JsonData)
					{
						const std::string UserName = JsonData["token"].s();

						// @TODO Change session time

						OutResponse = CreateResponse(crow::status::NOT_IMPLEMENTED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Not fully implemented."} });
					}
				}

				return OutResponse;
			});

	CROW_ROUTE(CrowApp, "/api/v1/users/logout")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			if (!AbuseProtectionPtr->IsAddressBlocked(ClientIP))
			{
				const crow::json::rvalue JsonData = crow::json::load(req.body);
				if (JsonData)
				{
					const std::string SessionToken = JsonData["token"].s();

					const bool bSuccessfullyLoggedOut = UserManager->Logout(SessionToken);
					if (bSuccessfullyLoggedOut)
					{
						OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Session terminated!"} });
					}
					else
					{
						OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Can not log out."} });
					}
				}
			}

			return OutResponse;
		});
}

void FProjectEngine::CreateSocketListener()
{
	/*

	FSocketAppWrapper SocketAppWrapper()

	if (bIsSSLEnabled)
	{
		WebSocketApp = uWS::SSLApp({
			// These are the most common options, fullchain and key. See uSockets for more options. 
			.cert_file_name = CertFilePath.c_str(),
			.key_file_name = KeyFilePath.c_str()
		});
	}
	else
	{
		WebSocketApp = uWS::App();
	}

	WebSocketApp.ws<FPerSocketData>("/api/v1/ws", {
		.open = [](auto* ws) {
			LOG_INFO("WebSocket connected!");
		},
		.message = [](auto* ws, std::string_view message, uWS::OpCode)
		{
			ws->send(message, uWS::OpCode::TEXT);
		},
		.close = [](auto* ws, int code, std::string_view message)
		{
			LOG_INFO("WebSocket closed!");
		}
	});

	WebSocketApp.listen(8081, [](auto* token)
	{
		if (token)
		{
			LOG_INFO("WebSocket server listening");
		}
	});

	WebSocketApp.run();

	LOG_INFO("WebSocket server finished.");
	*/
}

void FProjectEngine::StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	// Find port in settings
	constexpr uint16 ServerPortDefault = 8080;

	int32 ServerPort;
	bIsSSLEnabled = false;
	bool bDoesServerSettingsExist = ServerSettingsIni->DoesIniExist();
	if (bDoesServerSettingsExist)
	{
		const FIniField ServerPortField = ServerSettingsIni->FindFieldByName("Port");
		if (ServerPortField.IsValid())
		{
			ServerPort = ServerPortField.GetValueAsInt();
		}

		const FIniField EnableSSLField = ServerSettingsIni->FindFieldByName("EnableSSL");
		if (EnableSSLField.IsValid())
		{
			bIsSSLEnabled = EnableSSLField.GetValueAsBool();
		}

		if (bIsSSLEnabled)
		{
			const FAssetsManager* AssetsManager = FGlobalDefines::GEngine->GetAssetsManager();
			const std::string ConfigPathAbsolute = AssetsManager->ConvertRelativeToFullPath(AssetsManager->GetConfigPathRelative());

			const FIniField SSLKeyField = ServerSettingsIni->FindFieldByName("SSLKey");
			if (SSLKeyField.IsValid())
			{
				KeyFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLKeyField.GetValueAsString();
			}

			const FIniField SSLCertField = ServerSettingsIni->FindFieldByName("SSLCert");
			if (SSLCertField.IsValid())
			{
				CertFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLCertField.GetValueAsString();
			}
		}
	}
	else
	{
		ServerPort = ServerPortDefault;
	}

	if (bDoesServerSettingsExist && bIsSSLEnabled)
	{
		LOG_INFO("Server will start with SSL");

		if (FFileSystem::File::Exists(CertFilePath) && FFileSystem::File::Exists(KeyFilePath))
		{
			CrowAppFutureAsync = CrowApp.port(static_cast<Uint16>(ServerPort))
				.ssl_file(CertFilePath.c_str(), KeyFilePath.c_str())
				.multithreaded()
				.run_async();
		}
		else
		{
			LOG_ERROR("Backend will NOT start.");
			LOG_ERROR("Attempted to start crow with SSL but Key or Cert files are missing\n.");


			LOG_INFO("Expected paths:");
			LOG_INFO("CertFilePath: " << CertFilePath);
			LOG_INFO("KeyFilePath: " << KeyFilePath);

			LOG_INFO("\nTo generate for testing use bat script in Assets.");
		}
	}
	else
	{
		LOG_INFO("Server will start without SSL");

		CrowAppFutureAsync = CrowApp.port(static_cast<uint16>(ServerPort))
			.multithreaded()
			.run_async();
	}
}

void FProjectEngine::PreExit()
{
	FEngine::PreExit();

	CrowApp.stop();

	//CrowAppFutureAsync.wait();

	LOG_INFO("Stopped crow");
}

void FProjectEngine::AddHeaders(crow::response& CurrentResponse, const CUnorderedMap<std::string, std::string>& HeaderNameToValueMap)
{
	for (const std::pair<const std::string, std::string>& Value : HeaderNameToValueMap)
	{
		CurrentResponse.add_header(Value.first, Value.second);
	}
}

void FProjectEngine::AddCookies(crow::response& CurrentResponse, const std::string& AuthToken)
{
	if (bIsSSLEnabled)
	{
		// HTTP
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=86400");
	}
	else
	{
		// HTTPS
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
	}
}

std::optional<std::string> FProjectEngine::GetCookie(const crow::request& Req, std::string_view CookieName)
{
	std::string_view CookieHeader = Req.get_header_value("Cookie");

	if (CookieHeader.empty())
	{
		return std::nullopt;
	}

	// Find cookie name
	size_t Pos = CookieHeader.find(CookieName);
	if (Pos == std::string_view::npos)
	{
		return std::nullopt;
	}

	// Check if it's actually the cookie name (not part of another name)
	if (Pos > 0 && CookieHeader[Pos - 1] != ' ' && CookieHeader[Pos - 1] != ';')
	{
		return std::nullopt;
	}

	// Find '=' after cookie name
	size_t Start = Pos + CookieName.length();
	if (Start >= CookieHeader.length() || CookieHeader[Start] != '=')
	{
		return std::nullopt;
	}

	Start++; // Skip '='

	// Find end (';' or end of string)
	size_t End = CookieHeader.find(';', Start);

	if (End == std::string_view::npos)
	{
		return std::string(CookieHeader.substr(Start));
	}

	return std::string(CookieHeader.substr(Start, End - Start));
}

CUnorderedMap<std::string, std::string> FProjectEngine::GetDefaultHeaders() const
{
	CUnorderedMap<std::string, std::string> OutDefaultHeaders = AbuseProtectionPtr->GetCORHeaders();

	return OutDefaultHeaders;
}

crow::response FProjectEngine::CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields) const
{
	crow::json::wvalue response;

	for (const std::pair<const std::string, std::string>& JsonField : JsonFields)
	{
		response[JsonField.first] = JsonField.second;
	}

	return crow::response(ResponseCode, response);
}
