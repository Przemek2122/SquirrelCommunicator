// Created by Przemys³aw Wiewióra 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "CoreMinimal.h"
#include "Engine.h"

// Enable SSL Support in crow library
#define CROW_ENABLE_SSL
#include "BackendSettings.h"
#include "crow/app.h"

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

	void InitBasicSetup();
	void InitUsersSetup();
	void InitMessagesSetup();

	void StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni);

	void PreExit() override;

	crow::SimpleApp& GetCrowApp() { return CrowApp; }
	FBackendSettings* GetBackendSettings() const { return BackendSettings.get(); }

protected:
	crow::response CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields) const;

protected:
	/** API Server */
	crow::SimpleApp CrowApp;

	/** Class for managing users */
	std::unique_ptr<FUserManager> UserManager;

	/** Async for crow app */
	std::future<void> CrowAppFutureAsync;

	std::unique_ptr<FBackendSettings> BackendSettings;
	std::unique_ptr<FAbuseProtection> AbuseProtectionPtr;

};
