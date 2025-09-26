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
	CROW_ROUTE(CrowApp, "/")([]()
		{
			return "Crow C++ API Server is running.";
		});

	// Route for testing if api works
	CROW_ROUTE(CrowApp, "/api/v1/test")([]()
		{
			return "true";
		});
}

void FProjectEngine::InitUsersSetup()
{

	CROW_ROUTE(CrowApp, "/api/v1/users")([]()
		{
			return "no user specified";
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/register").methods("POST"_method)
		([this] (const crow::request& req)
		{
			const crow::json::rvalue json_data = crow::json::load(req.body);
			if (json_data)
			{
				const std::string UserName = json_data["username"].s();
				const std::string UserPassword = json_data["password"].s();
				const std::string EMail = json_data["email"].s();

				const ERegisterUserStatus RegisterStatus = UserManager->RegisterUser(UserName, UserPassword, EMail);
				if (RegisterStatus == ERegisterUserStatus::Successful)
				{
					return CreateResponse(200, { { "status", "success" }, { "message", "User registered successfully"} });
				}
				else
				{
					return CreateResponse(400, { { "status", "error" }, { "message","Registration failed. User may already exist or invalid input."} });
				}
			}
			else
			{
				return CreateResponse(400, { { "status", "error" }, { "message", "Invalid JSON."} });
			}
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/login")([]()
		{
			return "no user specified";
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
