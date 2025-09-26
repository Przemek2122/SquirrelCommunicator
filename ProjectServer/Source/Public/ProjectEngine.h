// Created by Przemys³aw Wiewióra 2020-2024 https://github.com/Przemek2122/Engine
#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "crow/app.h"

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

	void StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni);

protected:
	crow::response CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields) const;

protected:
	/** API Server */
	crow::SimpleApp CrowApp;

	/** Class for managing users */
	std::unique_ptr<FUserManager> UserManager;

};
