#include "ProjectEngine.h"

#include "PredefinedMessages.h"
#include "Auth/UserManager.h"
#include "Assets/IniReader/IniManager.h"
#include "Assets/IniReader/IniObject.h"
#include "Threads/ThreadsManager.h"

FProjectEngine::FProjectEngine()
	: UserManager(new FUserManager())
{
}

void FProjectEngine::Init()
{
	FEngine::Init();

	LOG_DEBUG("Server init");

	FIniManager* IniManager = FGlobalDefines::GEngine->GetAssetsManager()->GetIniManager();
	std::shared_ptr<FIniObject> ServerSettingsIni = IniManager->GetIniObject("ServerSettings");
	if (ServerSettingsIni->DoesIniExist())
	{
		ServerSettingsIni->LoadIni();

		InitBasicSetup();

		LOG_DEBUG("Created api test");

		InitUsersSetup();

		LOG_DEBUG("Created api user");

		InitMessagesSetup();

		LOG_DEBUG("Created api messages");

		UserManager->Init();
		StartServer(ServerSettingsIni);
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
			return CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Crow C++ API Server is running."} });
		});

	// Route for testing if api works
	CROW_ROUTE(CrowApp, "/api/v1/test")([this]()
		{
			return CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
		});
}

void FProjectEngine::InitUsersSetup()
{
	CROW_ROUTE(CrowApp, "/api/v1/users")([this]()
		{
			return CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Wrong API Request."} });
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/register")
		.methods("POST"_method)
		([this] (const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["username"].s();
				const std::string UserPassword = JsonData["password"].s();
				const std::string EMail = JsonData["email"].s();

				const ERegisterUserStatus RegisterStatus = UserManager->RegisterUser(UserName, UserPassword, EMail);
				if (RegisterStatus == ERegisterUserStatus::Successful)
				{
					OutResponse = CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User registered successfully."} });
				}
				else
				{
					OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/login")
		.methods("POST"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["username"].s();
				const std::string UserPassword = JsonData["password"].s();

				std::string OutSessionToken;
				const ELoginStatus LoginStatus = UserManager->LoginUser(UserName, UserPassword, OutSessionToken);

				if (!OutSessionToken.empty())
				{
					if (LoginStatus == ELoginStatus::Successful)
					{
						OutResponse = CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User login successful!"}, { "token", OutSessionToken } });
					}
					else if (LoginStatus == ELoginStatus::SessionAlreadyExist)
					{
						OutResponse = CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Session already exists!"} });
					}
				}
				else
				{
					OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Unable to generate session."} });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/refresh")
		.methods("POST"_method)
		([this](const crow::request& req)
			{
				crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

				const crow::json::rvalue JsonData = crow::json::load(req.body);
				if (JsonData)
				{
					const std::string UserName = JsonData["token"].s();

					// @TODO Add session terminate

					OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Not fully implemented."} });
				}

				return OutResponse;
			});

	CROW_ROUTE(CrowApp, "/api/v1/users/logout")
		.methods("POST"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string SessionToken = JsonData["token"].s();

				const bool bSuccessfullyLoggedOut = UserManager->Logout(SessionToken);
				if (bSuccessfullyLoggedOut)
				{
					OutResponse = CreateResponse(200, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Session terminated!"} });
				}
				else
				{
					OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Can not log out."} });
				}
			}

			return OutResponse;
		});
}

void FProjectEngine::InitMessagesSetup()
{
	// WebSocket endpoint
	CROW_WEBSOCKET_ROUTE(CrowApp, "/api/v1/ws")
		.onaccept([&](const crow::request& Req, void** UserData)
		{
			bool bCanConnect = false;

			const std::string AuthorizationString = Req.get_header_value("Authorization");
			const std::string AuthorizationStringWithoutBearer = AuthorizationString.substr(7);
			if (!AuthorizationStringWithoutBearer.empty())
			{
				// TODO Check token

				bCanConnect = true;
			}

			return bCanConnect;
		})
		.onopen([&](crow::websocket::connection& conn)
		{
			CROW_LOG_INFO << "New WebSocket connection";
		})
		.onclose([&](crow::websocket::connection& Conn, const std::string& Reason, uint16_t WithStatusCode)
		{
			CROW_LOG_INFO << "Connection closed: " << Reason;
		})
		.onerror([&](crow::websocket::connection& conn, const std::string& ErrorMessage)
		{
			CROW_LOG_INFO << "Connection error: " << ErrorMessage;
		})
		.onmessage([&](crow::websocket::connection& Conn, const std::string& Message, bool bIsBinary)
		{
			// Handle incoming message
			Conn.send_text("Echo: " + Message);
		});
}

void FProjectEngine::StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	// Find port in settings
	constexpr uint16 ServerPortDefault = 8080;

	int32 ServerPort;
	std::string KeyFilePath;
	std::string CertFilePath;
	bool bShouldEnableSSL = false;
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
			bShouldEnableSSL = EnableSSLField.GetValueAsBool();
		}

		if (bShouldEnableSSL)
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

	if (bDoesServerSettingsExist && bShouldEnableSSL)
	{
		LOG_INFO("Server will start with SSL");

		CrowAppFutureAsync = CrowApp.port(static_cast<Uint16>(ServerPort))
			.ssl_file(CertFilePath.c_str(), KeyFilePath.c_str())
			.multithreaded()
			.run_async();
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

crow::response FProjectEngine::CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields) const
{
	crow::json::wvalue response;

	for (const std::pair<const std::string, std::string>& JsonField : JsonFields)
	{
		response[JsonField.first] = JsonField.second;
	}

	return crow::response(ResponseCode, response);
}
