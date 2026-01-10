#include "ProjectEngine.h"

#include "PredefinedMessages.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Assets/IniReader/IniObject.h"
#include "DataBase/DataBaseSettings.h"
#include "Managers/ConversationsManager.h"
#include "Rest/CrowUtils.h"
#include "Rest/TestEndpoint.h"
#include "Rest/UserEndpoint.h"
#include "Sockets/SocketManager.h"

FProjectEngine::FProjectEngine()
	: BackendSettings(std::make_unique<FBackendSettings>())
	, SocketManager(std::make_unique<FSocketManager>())
	, ConversationsManager(std::make_unique<FConversationsManager>())
	, bIsSSLEnabled(false)
{
	// Collect Database settings
	FDataBaseSettings::Initialize();

	RestEndpointsClasses.Push(FClassStorage<FCrowAppEndpoint, FProjectEngine*>().InlineSet<FTestEndpoint>());
	RestEndpointsClasses.Push(FClassStorage<FCrowAppEndpoint, FProjectEngine*>().InlineSet<FUserEndpoint>());
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
		// Get domain name from ini
		{
			const FIniField DomainField = ServerSettingsIni->FindFieldByName("Domain");
			if (DomainField.IsValid())
			{
				DomainName = DomainField.GetValueAsString();
			}
		}

		// Read all backend addresses from ini
		{
			FIniField BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress" + std::to_string(1));
			for (int32 i = 2; i < 128 && BackendAddressField.IsValid(); i++)
			{
				OriginWhitelist.Push(BackendAddressField.GetValueAsString());
				BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress" + std::to_string(i));
			}
		}

#if DEBUG
		{
			// Debug domain
			FIniField DebugDomainField = ServerSettingsIni->FindFieldByName("DebugDomain");

			OriginWhitelist.InsertAt(0 ,"http://" + DebugDomainField.GetValueAsString());
			DomainName = DebugDomainField.GetValueAsString();
		}
#endif

		// Create endpoints
		for (auto& RestEndpointsClass : RestEndpointsClasses)
		{
			FCrowAppEndpoint* CrowAppEndpoint = RestEndpointsClass.Allocate(this);
			std::shared_ptr<FCrowAppEndpoint> SharedPtr = std::make_shared<FCrowAppEndpoint>(*CrowAppEndpoint);
			CrowAppEndpoint->RegisterRoutes(CrowApp);
			RestEndpointInstances.Push(SharedPtr);
		}

		UserManager->Init();

		// HTTP/REST crow server
		StartServer(ServerSettingsIni);

#if !defined(DEBUG) || !DEBUG
		// Set proper log level in non debug builds for performance
		CrowApp.loglevel(crow::LogLevel::Warning);
#endif

		// Socket
		const FIniField PortWSField = ServerSettingsIni->FindFieldByName("PortWS");
		const FIniField SocketListenHostField = ServerSettingsIni->FindFieldByName("SocketListenHost");
		if (PortWSField.IsValid() && SocketListenHostField.IsValid())
		{
			SocketManager->CreateSockets(SocketListenHostField.GetValueAsString(), PortWSField.GetValueAsInt(), bIsSSLEnabled, KeyFilePath, CertFilePath);
		}
		else
		{
			LOG_WARN("Missing PortWSField or SocketListenHostField. Sockets will not work.");
		}
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

void FProjectEngine::InitUsersSetup()
{
	
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

			bool bIsPathAbsolute = false;
			const FIniField SSLPathsAbsoluteField = ServerSettingsIni->FindFieldByName("SSLPathsAbsolute");
			if (SSLPathsAbsoluteField.IsValid())
			{
				bIsPathAbsolute = SSLPathsAbsoluteField.GetValueAsBool();
			}

			const FIniField SSLKeyField = ServerSettingsIni->FindFieldByName("SSLKey");
			if (SSLKeyField.IsValid())
			{
				if (bIsPathAbsolute)
				{
					KeyFilePath = SSLKeyField.GetValueAsString();
				}
				else
				{
					KeyFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLKeyField.GetValueAsString();
				}
			}

			const FIniField SSLCertField = ServerSettingsIni->FindFieldByName("SSLCert");
			if (SSLCertField.IsValid())
			{
				if (bIsPathAbsolute)
				{
					CertFilePath = SSLCertField.GetValueAsString();
				}
				else
				{
					CertFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLCertField.GetValueAsString();
				}
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
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Domain=" + DomainName + "; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=86400");
	}
	else
	{
		// HTTPS
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Domain=" + DomainName + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
	}
}

CUnorderedMap<std::string, std::string> FProjectEngine::GetDefaultHeaders() const
{
	CUnorderedMap<std::string, std::string> OutDefaultHeaders = AbuseProtectionPtr->GetCORHeaders();

	return OutDefaultHeaders;
}
