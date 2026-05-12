#include "ProjectEngine.h"

#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Assets/IniReader/IniObject.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "Managers/ConversationsManager.h"
#include "Managers/PasswordResetManager.h"
#include "Managers/RoomsServiceManager.h"
#include "Rest/AccountEndpoint.h"
#include "Rest/CrowUtils.h"
#include "Rest/IntegrationEndpoint.h"
#include "Rest/TestEndpoint.h"
#include "Rest/AuthEndpoint.h"
#include "Sockets/SocketManager.h"

#define ENDPOINT_CLASS(EndpointName) FClassStorage<FCrowAppEndpoint, FProjectEngine*>().InlineSet<EndpointName>()

FProjectEngine::FProjectEngine()
	: BackendSettings(std::make_unique<FBackendSettings>())
	, SocketManager(std::make_unique<FSocketManager>())
	, ConversationsManager(std::make_unique<FConversationsManager>())
	, PasswordResetManager(nullptr)
	, bIsSSLEnabled(false)
{
	// Collect Database settings
	FDataBaseSettings::Initialize();

	RestEndpointsClasses.Push(ENDPOINT_CLASS(FTestEndpoint));
	RestEndpointsClasses.Push(ENDPOINT_CLASS(FAuthEndpoint));
	RestEndpointsClasses.Push(ENDPOINT_CLASS(FIntegrationEndpoint));
	RestEndpointsClasses.Push(ENDPOINT_CLASS(FAccountEndpoint));
}

void FProjectEngine::Init()
{
	/** We do not need SDL input in server */
	DisableInput();

	FEngine::Init();

	LOG_DEBUG("Server init");

	UserManager = std::make_unique<FUserManager>();

	BackendSettings->LoadBackendSettings();
	std::shared_ptr<FIniObject> ServerSettingsIni = BackendSettings->GetBackendSettingsIni();
	if (ServerSettingsIni->DoesIniExist())
	{
		// Read limits from settings or use defaults
		int32 MaxSentRequests = 10;
		int32 MaxIncomingRequests = 10;
		int32 MaxFriends = 25;

		const FIniField MaxSentRequestsField = ServerSettingsIni->FindFieldByName("MaxSentRequests");
		if (MaxSentRequestsField.IsValid())
		{
			MaxSentRequests = MaxSentRequestsField.GetValueAsInt();
		}

		const FIniField MaxIncomingRequestsField = ServerSettingsIni->FindFieldByName("MaxIncomingRequests");
		if (MaxIncomingRequestsField.IsValid())
		{
			MaxIncomingRequests = MaxIncomingRequestsField.GetValueAsInt();
		}

		const FIniField MaxFriendsField = ServerSettingsIni->FindFieldByName("MaxFriends");
		if (MaxFriendsField.IsValid())
		{
			MaxFriends = MaxFriendsField.GetValueAsInt();
		}

		FriendListManager = std::make_unique<FFriendListManager>(GetThreadsManager(), MaxSentRequests, MaxIncomingRequests, MaxFriends);
	}

	AbuseProtectionPtr = std::make_unique<FAbuseProtection>(BackendSettings.get());
	DefaultHeadersCache = GetDefaultHeaders();
	RoomsManager = std::make_unique<FRoomsServiceManager>();

	if (ServerSettingsIni->DoesIniExist())
	{
		// Get time for token to be alive
		const FIniField PasswordResetTokenAliveTimeMinsField = ServerSettingsIni->FindFieldByName("PasswordResetTokenAliveTimeMins");
		int32 PasswordResetTokenAliveTimeMins = 10;
		if (PasswordResetTokenAliveTimeMinsField.IsValid())
		{
			PasswordResetTokenAliveTimeMins = PasswordResetTokenAliveTimeMinsField.GetValueAsInt();
		}

		PasswordResetManager = std::make_unique<FPasswordResetManager>(PasswordResetTokenAliveTimeMins);
		PasswordResetManager->Init();

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

		CacheProperties(ServerSettingsIni);

		// Test db connection
		TestDataBaseConnection();
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

void FProjectEngine::CacheProperties(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	const char* MailAPIKeyPropertyName = "SQRLL_COMM_MAIL_API_KEY";
	char* Variable = std::getenv(MailAPIKeyPropertyName);
	if (Variable != nullptr)
	{
		MailAPIKey = Variable;
	}

	if (MailAPIKey.empty())
	{
		LOG_WARN("Env property: '" << MailAPIKeyPropertyName << "' - missing. App will work but password reset will fail. It can be generated on Brevo page. (Section 'https://app.brevo.com/settings/keys/api')");
	}
}

void FProjectEngine::TestDataBaseConnection()
{
	bool DBConnectionSuccessful = false;
	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		soci::session& DataBaseSession = Connect.GetSession();

		try
		{
			int result;
			DataBaseSession << "SELECT 1", soci::into(result);

			LOG_INFO("Database has connection.");
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Database error: " << e.what());
		}
	}
	else
	{
		LOG_ERROR("Database missing connection.");
	}
}

CUnorderedMap<std::string, std::string> FProjectEngine::GetDefaultHeaders() const
{
	CUnorderedMap<std::string, std::string> OutDefaultHeaders = AbuseProtectionPtr->GetCORHeaders();

	return OutDefaultHeaders;
}
