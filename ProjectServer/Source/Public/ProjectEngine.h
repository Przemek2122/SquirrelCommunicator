// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "EngineCompat.h"
#include "Logger/Logger.h"
#include "Rest/CrowAppMiddleware.h"
#include "Rest/CrowAppEndpoint.h"
#include "BackendSettings.h"
#include "crow/app.h"
#include "Managers/FriendListManager.h"

#include <functional>
#include <memory>
#include <future>
#include <string>
#include <vector>

class FServersManager;
class FTransferTokenManager;
class FRoomsServiceManager;
class FPasswordResetManager;
class FEmailVerificationManager;
class FConversationsManager;
class FSocketManager;
class FAbuseProtection;
class FUserManager;

// Factory function type for REST endpoints (replaces FClassStorage)
using FEndpointFactory = std::function<FCrowAppEndpoint*(FProjectEngine*)>;

/**
 * Primary engine class for your project.
 * Standalone version - no longer inherits from FEngine.
 */
class FProjectEngine
{
public:
	FProjectEngine();
	~FProjectEngine();

	void Init();
	void PostSecondTick();

	void StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni);

	void PreExit();

	void AddHeaders(crow::response& CurrentResponse, const CUnorderedMap<std::string, std::string>& HeaderNameToValueMap);
	void AddCookies(crow::response& CurrentResponse, const std::string& AuthToken);
	std::string ExtractCookieValue(const std::string& CookieHeader, const std::string& CookieName);

	void CacheProperties(const std::shared_ptr<FIniObject>& ServerSettingsIni);
	void TestDataBaseConnection();

	FUserManager* GetUserManager() const { return UserManager.get(); }
	FTransferTokenManager* GetTransferTokenManager() const { return TransferTokenManager.get(); }
	FConversationsManager* GetConversationsManager() const { return ConversationsManager.get(); }
	FServersManager* GetServersManager() const { return ServersManager.get(); }
	FSocketManager* GetSocketManager() const { return SocketManager.get(); }

	crow::App<FCrowAppMiddleware>& GetCrowApp() { return CrowApp; }
	FBackendSettings* GetBackendSettings() const { return BackendSettings.get(); }
	FAbuseProtection* GetAbuseProtection() const { return AbuseProtectionPtr.get(); }
	FPasswordResetManager* GetPasswordResetManager() const { return PasswordResetManager.get(); }
	FEmailVerificationManager* GetEmailVerificationManager() const { return EmailVerificationManager.get(); }
	FFriendListManager* GetFriendListManager() const { return FriendListManager.get(); }
	FRoomsServiceManager* GetRoomsManager() const { return RoomsManager.get(); }

	CUnorderedMap<std::string, std::string> GetDefaultHeaders() const;
	CUnorderedMap<std::string, std::string> GetDefaultHeadersCache() const { return DefaultHeadersCache; }
	const CArray<std::string>& GetOriginWhitelist() const { return OriginWhitelist; }
	std::string GetMailAPIKey() const { return MailAPIKey; }

	/**
	 * Base URL used when building public-facing links (registration
	 * verification emails, invite links, etc.).
	 *
	 * Debug builds target the local REST server (http://<DebugDomain>:<Port>,
	 * e.g. http://localhost:8080), while release builds use BackendAddress1
	 * from the INI (e.g. https://comm.sqrll.net).
	 */
	std::string GetPublicBaseUrl() const { return PublicBaseUrl; }

protected:

	/** API Server */
	crow::App<FCrowAppMiddleware> CrowApp;

	/** Async for crow app */
	std::future<void> CrowAppFutureAsync;

	/** Class for managing users */
	std::unique_ptr<FUserManager> UserManager;

	/** Class for managing transfer tokens */
	std::unique_ptr<FTransferTokenManager> TransferTokenManager;

	/** Abuse protection, Rate limit, cors */
	std::unique_ptr<FAbuseProtection> AbuseProtectionPtr;

	/** Backed settings contains ini with settings for backend */
	std::unique_ptr<FBackendSettings> BackendSettings;

	/** Web socket manager */
	std::unique_ptr<FSocketManager> SocketManager;

	/** Conversations manager */
	std::unique_ptr<FConversationsManager> ConversationsManager;

	/** Servers manager */
	std::unique_ptr<FServersManager> ServersManager;

	/** PasswordResetManager */
	std::unique_ptr<FPasswordResetManager> PasswordResetManager;

	/** Email verification manager for registration codes */
	std::unique_ptr<FEmailVerificationManager> EmailVerificationManager;

	/** FriendListManager */
	std::unique_ptr<FFriendListManager> FriendListManager;

	/** Manager for rooms service (GO Microserivce) */
	std::unique_ptr<FRoomsServiceManager> RoomsManager;

	/** Array of rest endpoint factory functions (replaces FClassStorage) */
	CArray<FEndpointFactory> RestEndpointsFactories;

	/** Array with rest endpoints instances */
	CArray<std::shared_ptr<FCrowAppEndpoint>> RestEndpointInstances;

	/** Cached default headers */
	CUnorderedMap<std::string, std::string> DefaultHeadersCache;

	/** Cached API Key for sending emails */
	std::string MailAPIKey;

	CArray<std::string> OriginWhitelist;
	std::string DomainName;

	/** Base URL for public links (scheme + host + optional port). */
	std::string PublicBaseUrl;

	bool bIsSSLEnabled;
	std::string KeyFilePath;
	std::string CertFilePath;

	/** Seconds since last DB pool keepalive cycle */
	Uint64 SecondsSinceLastPoolKeepAlive = 0;
};
