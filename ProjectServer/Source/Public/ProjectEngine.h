// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Rest/CrowAppMiddleware.h"
#include "Rest/CrowAppEndpoint.h"
#include "BackendSettings.h"
#include "crow/app.h"

class FPasswordResetManager;
class FConversationsManager;
class FSocketManager;
class FAbuseProtection;
class FUserManager;

/**
 * Primary engine class for your project.
 */
class FProjectEngine : public FEngine
{
public:
	FProjectEngine();

	void Init() override;
	void PostSecondTick() override;

	void InitUsersSetup();

	void StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni);

	void PreExit() override;

	void AddHeaders(crow::response& CurrentResponse, const CUnorderedMap<std::string, std::string>& HeaderNameToValueMap);
	void AddCookies(crow::response& CurrentResponse, const std::string& AuthToken);

	void CacheProperties(const std::shared_ptr<FIniObject>& ServerSettingsIni);
	void TestDataBaseConnection();

	FUserManager* GetUserManager() const { return UserManager.get(); }
	FConversationsManager* GetConversationsManager() const { return ConversationsManager.get(); }
	FSocketManager* GetSocketManager() const { return SocketManager.get(); }

	crow::App<FCrowAppMiddleware>& GetCrowApp() { return CrowApp; }
	FBackendSettings* GetBackendSettings() const { return BackendSettings.get(); }
	FAbuseProtection* GetAbuseProtection() const { return AbuseProtectionPtr.get(); }
	FPasswordResetManager* GetPasswordResetManager() const { return PasswordResetManager.get(); }

	CUnorderedMap<std::string, std::string> GetDefaultHeaders() const;
	CUnorderedMap<std::string, std::string> GetDefaultHeadersCache() const { return DefaultHeadersCache; }
	const CArray<std::string>& GetOriginWhitelist() const { return OriginWhitelist; }
	std::string GetMailAPIKey() const { return MailAPIKey; }

protected:
	/** API Server */
	crow::App<FCrowAppMiddleware> CrowApp;

	/** Async for crow app */
	std::future<void> CrowAppFutureAsync;

	/** Class for managing users */
	std::unique_ptr<FUserManager> UserManager;

	/** Abuse protection, Rate limit, cors */
	std::unique_ptr<FAbuseProtection> AbuseProtectionPtr;

	/** Backed settings contains ini with settings for backend */
	std::unique_ptr<FBackendSettings> BackendSettings;

	/** Web socket manager */
	std::unique_ptr<FSocketManager> SocketManager;

	/** Conversations manager */
	std::unique_ptr<FConversationsManager> ConversationsManager;

	/** PasswordResetManager */
	std::unique_ptr<FPasswordResetManager> PasswordResetManager;

	/** Array of rest endpoint classes */
	CArray<FClassStorage<FCrowAppEndpoint, FProjectEngine*>> RestEndpointsClasses;

	/** Array with rest endpoints instances */
	CArray<std::shared_ptr<FCrowAppEndpoint>> RestEndpointInstances;

	/** Cached default headers */
	CUnorderedMap<std::string, std::string> DefaultHeadersCache;

	/** Cached API Key for sending emails */
	std::string MailAPIKey;

	CArray<std::string> OriginWhitelist;
	std::string DomainName;

	bool bIsSSLEnabled;
	std::string KeyFilePath;
	std::string CertFilePath;

};
