#include "ProjectEngine.h"

#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "ThreadCompat.h"
#include "SQRLLIniObject.h"
#include "Auth/TransferTokenManager.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "Managers/ConversationsManager.h"
#include "Managers/PasswordResetManager.h"
#include "Managers/EmailVerificationManager.h"
#include "Managers/RoomsServiceManager.h"
#include "Managers/ServersManager.h"
#include "Rest/AccountEndpoint.h"
#include "Rest/CrowUtils.h"
#include "Rest/IntegrationEndpoint.h"
#include "Rest/TestEndpoint.h"
#include "Rest/AuthEndpoint.h"
#include "Rest/TransferTokenEndpoint.h"
#include "Sockets/SocketManager.h"

#include <filesystem>

// Endpoint factory macro - replaces FClassStorage
#define ENDPOINT_FACTORY(EndpointName) FEndpointFactory([](FProjectEngine* engine) -> FCrowAppEndpoint* { return new EndpointName(engine); })

// How often (in seconds) the DB pool is scanned for dead connections
// and reconnected.  1800 = 30 minutes.
static constexpr Uint64 DB_POOL_KEEPALIVE_INTERVAL_SEC = 1800;

FProjectEngine::FProjectEngine()
	: BackendSettings(std::make_unique<FBackendSettings>())
	, SocketManager(std::make_unique<FSocketManager>())
	, ConversationsManager(std::make_unique<FConversationsManager>())
	, ServersManager(std::make_unique<FServersManager>())
	, PasswordResetManager(nullptr)
	, EmailVerificationManager(nullptr)
	, bIsSSLEnabled(false)
{
	// Collect Database settings
	FDataBaseSettings::Initialize();

	// Initialize DB connection pool (must be after settings are loaded)
	FDataBaseConnect::InitPool(GetNumberOfLogicalCPU());

	RestEndpointsFactories.Push(ENDPOINT_FACTORY(FTestEndpoint));
	RestEndpointsFactories.Push(ENDPOINT_FACTORY(FAuthEndpoint));
	RestEndpointsFactories.Push(ENDPOINT_FACTORY(FIntegrationEndpoint));
	RestEndpointsFactories.Push(ENDPOINT_FACTORY(FTransferTokenEndpoint));
	RestEndpointsFactories.Push(ENDPOINT_FACTORY(FAccountEndpoint));
}

// Destructor must be defined here where all unique_ptr types are complete
FProjectEngine::~FProjectEngine() = default;

void FProjectEngine::Init()
{
	// Set global engine pointer for legacy FGlobalDefines::GEngine access
	FGlobalDefines::GEngine = this;

	LOG_DEBUG("Server init");

	BackendSettings->LoadBackendSettings();
	std::shared_ptr<FIniObject> ServerSettingsIni = BackendSettings->GetBackendSettingsIni();
	if (ServerSettingsIni && ServerSettingsIni->IsLoaded())
	{
		const FIniField SessionLifeTimeField = ServerSettingsIni->FindFieldByName("SessionLifeTime");
		Uint64 SessionLifeTime = 1000;
		if (SessionLifeTimeField.IsValid())
		{
			SessionLifeTime = SessionLifeTimeField.GetValueAsInt();
		}

		UserManager = std::make_unique<FUserManager>(SessionLifeTime);
		TransferTokenManager = std::make_unique<FTransferTokenManager>();

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

		FriendListManager = std::make_unique<FFriendListManager>(MaxSentRequests, MaxIncomingRequests, MaxFriends);
		AbuseProtectionPtr = std::make_unique<FAbuseProtection>(BackendSettings.get());
		DefaultHeadersCache = GetDefaultHeaders();
		RoomsManager = std::make_unique<FRoomsServiceManager>();

		// Get time for token to be alive
		const FIniField PasswordResetTokenAliveTimeMinsField = ServerSettingsIni->FindFieldByName("PasswordResetTokenAliveTimeMins");
		int32 PasswordResetTokenAliveTimeMins = 10;
		if (PasswordResetTokenAliveTimeMinsField.IsValid())
		{
			PasswordResetTokenAliveTimeMins = PasswordResetTokenAliveTimeMinsField.GetValueAsInt();
		}

		PasswordResetManager = std::make_unique<FPasswordResetManager>(PasswordResetTokenAliveTimeMins);
		PasswordResetManager->Init();
		// Get time for registration verification code to be alive
		const FIniField RegistrationCodeAliveTimeMinsField = ServerSettingsIni->FindFieldByName("RegistrationCodeAliveTimeMins");
		int32 RegistrationCodeAliveTimeMins = 30;
		if (RegistrationCodeAliveTimeMinsField.IsValid())
		{
			RegistrationCodeAliveTimeMins = RegistrationCodeAliveTimeMinsField.GetValueAsInt();
		}

		EmailVerificationManager = std::make_unique<FEmailVerificationManager>(RegistrationCodeAliveTimeMins);
		EmailVerificationManager->Init();

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

		// Base URL used for public-facing links (registration verification
		// emails, invite links, etc.). Release builds use BackendAddress1;
		// debug builds target the local REST server on <DebugDomain>:<Port>
		// without SSL.
		{
			const FIniField BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress1");
			if (BackendAddressField.IsValid())
			{
				PublicBaseUrl = BackendAddressField.GetValueAsString();
			}
		}

#if DEBUG
		{
			// Debug domain override (defaults to localhost). This keeps the
			// verification/invite links, CORS whitelist and cookie domain all
			// consistent with the configured debug host.
			std::string DebugDomainName = "localhost";
			const FIniField DebugDomainField = ServerSettingsIni->FindFieldByName("DebugDomain");
			if (DebugDomainField.IsValid() && !DebugDomainField.GetValueAsString().empty())
			{
				DebugDomainName = DebugDomainField.GetValueAsString();
			}

			OriginWhitelist.InsertAt(0, "http://" + DebugDomainName);
			DomainName = DebugDomainName;

			// Local development: REST server listens on <DebugDomain>:<Port>
			// without SSL, so the public base URL must include the port.
			int32 DebugPort = 8080;
			const FIniField PortField = ServerSettingsIni->FindFieldByName("Port");
			if (PortField.IsValid())
			{
				DebugPort = PortField.GetValueAsInt();
			}
			PublicBaseUrl = "http://" + DebugDomainName + ":" + std::to_string(DebugPort);
		}
#endif

		// Create endpoints using factory functions
		for (auto& EndpointFactory : RestEndpointsFactories)
		{
			FCrowAppEndpoint* CrowAppEndpoint = EndpointFactory(this);
			std::shared_ptr<FCrowAppEndpoint> SharedPtr = std::shared_ptr<FCrowAppEndpoint>(CrowAppEndpoint);
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
	UserManager->PostSecondTick();
	TransferTokenManager->PostSecondTick();
	ServersManager->PostSecondTick();

	// Periodically scan the DB pool for dead connections and reconnect them.
	// At borrow-time, dead connections fall back to standalone connections,
	// so this is a best-effort optimization — not a correctness requirement.
	SecondsSinceLastPoolKeepAlive++;
	if (SecondsSinceLastPoolKeepAlive >= DB_POOL_KEEPALIVE_INTERVAL_SEC)
	{
		FDataBaseConnect::KeepPoolAlive();
		SecondsSinceLastPoolKeepAlive = 0;
	}
}

void FProjectEngine::StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	// Find port in settings
	constexpr uint16_t ServerPortDefault = 8080;

	int32 ServerPort = ServerPortDefault;
	bIsSSLEnabled = false;
	bool bDoesServerSettingsExist = ServerSettingsIni && ServerSettingsIni->IsLoaded();
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
			// Use ./Assets/Config/ as the config path (replaces Engine's AssetsManager)
			const std::string ConfigPathAbsolute = "./Assets/Config";

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
					KeyFilePath = ConfigPathAbsolute + "/" + SSLKeyField.GetValueAsString();
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
					CertFilePath = ConfigPathAbsolute + "/" + SSLCertField.GetValueAsString();
				}
			}
		}
	}

	if (bDoesServerSettingsExist && bIsSSLEnabled)
	{
		LOG_INFO("REST server (CrowCPP) will start with SSL");

		if (std::filesystem::exists(CertFilePath) && std::filesystem::exists(KeyFilePath))
		{
			CrowAppFutureAsync = CrowApp.port(static_cast<Uint16>(ServerPort))
				.ssl_file(CertFilePath, KeyFilePath)
				.multithreaded()
				.run_async();
		}
		else
		{
			LOG_ERROR("Backend will NOT start.");
			LOG_ERROR("Attempted to start crow with SSL but Key or Cert files are missing\n.");


			LOG_STATE("Expected paths:");
			LOG_STATE("CertFilePath: " << CertFilePath);
			LOG_STATE("KeyFilePath: " << KeyFilePath);

			LOG_STATE("\nTo generate for testing use bat script in Assets.");
		}
	}
	else
	{
		LOG_STATE("REST server (CrowCPP) will start without SSL");

		CrowAppFutureAsync = CrowApp.port(static_cast<uint16_t>(ServerPort))
			.multithreaded()
			.run_async();
	}
}

void FProjectEngine::PreExit()
{
	CrowApp.stop();

	//CrowAppFutureAsync.wait();

	LOG_INFO("Stopped crow");

	// Release all pooled DB connections
	FDataBaseConnect::ShutdownPool();
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

std::string FProjectEngine::ExtractCookieValue(const std::string& CookieHeader, const std::string& CookieName)
{
	const std::string SearchString = CookieName + "=";
	const size_t StartPos = CookieHeader.find(SearchString);
	if (StartPos == std::string::npos)
	{
		return "";
	}

	const size_t ValueStart = StartPos + SearchString.length();
	size_t EndPos = CookieHeader.find(';', ValueStart);
	if (EndPos == std::string::npos)
	{
		EndPos = CookieHeader.length();
	}

	return CookieHeader.substr(ValueStart, EndPos - ValueStart);
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

			LOG_STATE("Database has connection.");
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
