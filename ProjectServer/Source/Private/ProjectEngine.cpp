#include "ProjectEngine.h"
#include "UserManager.h"
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
			return CreateResponse(200, { { "status", "success" }, { "message", "Crow C++ API Server is running."} });
		});

	// Route for testing if api works
	CROW_ROUTE(CrowApp, "/api/v1/test")([this]()
		{
			return CreateResponse(200, { { "status", "success" }, { "message", "API is working."} });
		});
}

void FProjectEngine::InitUsersSetup()
{

	CROW_ROUTE(CrowApp, "/api/v1/users")([this]()
		{
			return CreateResponse(400, { { "status", "error" }, { "message", "Wrong API Request."} });
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/register")
		.methods("POST"_method)
		([this] (const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["username"].s();
				const std::string UserPassword = JsonData["password"].s();
				const std::string EMail = JsonData["email"].s();

				const ERegisterUserStatus RegisterStatus = UserManager->RegisterUser(UserName, UserPassword, EMail);
				if (RegisterStatus == ERegisterUserStatus::Successful)
				{
					OutResponse = CreateResponse(200, { { "status", "success" }, { "message", "User registered successfully"} });
				}
				else
				{
					OutResponse = CreateResponse(400, { { "status", "error" }, { "message","Registration failed. User may already exist or invalid input."} });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/login")
		.methods("POST"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Invalid JSON."} });

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
						OutResponse = CreateResponse(200, { { "status", "success" }, { "message", "User login successful!"}, { "token", OutSessionToken } });
					}
					else if (LoginStatus == ELoginStatus::SessionAlreadyExist)
					{
						OutResponse = CreateResponse(200, { { "status", "error" }, { "message", "Session already exists!"} });
					}
				}
				else
				{
					OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Unable to generate session."} });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/refresh_token")
		.methods("POST"_method)
		([this](const crow::request& req)
			{
				crow::response OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Invalid JSON."} });

				const crow::json::rvalue JsonData = crow::json::load(req.body);
				if (JsonData)
				{
					const std::string UserName = JsonData["token"].s();

					OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Not fully implemented."} });
				}

				return OutResponse;
			});

	CROW_ROUTE(CrowApp, "/api/v1/users/logout")
		.methods("POST"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["token"].s();

				OutResponse = CreateResponse(400, { { "status", "error" }, { "message", "Not fully implemented."} });
			}

			return OutResponse;
		});
}

void FProjectEngine::StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	// Find port in settings
	constexpr uint16 ServerPortDefault = 8080;
	int32 ServerPort;
	if (ServerSettingsIni)
	{
		const FIniField ServerPortField = ServerSettingsIni->FindFieldByName("Port");
		ServerPort = ServerPortField.GetValueAsInt();
	}
	else
	{
		ServerPort = ServerPortDefault;
	}

	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	FThreadData* CrowThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>("CrowThread");
	FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(CrowThreadData->GetThread());
	if (GenericThread != nullptr)
	{
		GenericThread->AddTask([this, ServerPort]()
		{
			CrowApp.port(static_cast<uint16>(ServerPort)).multithreaded().run();
		});

		LOG_DEBUG("Started server at port: '" << ServerPort << "'.");
		LOG_DEBUG("Go to localhost:" << ServerPort << "\\");
	}
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
