// Created by Przemys³aw Wiewióra 2020-2024 https://github.com/Przemek2122/Engine
#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Engine.h"
#include "crow/app.h"

class FProjectGameMode;
class FGameMode;
class FTextWidget;

/**
 * Primary engine class for your project.
 */
class FProjectEngine : public FEngine
{
public:
	FProjectEngine();

	void Init() override;
	void Tick() override;
	void PostSecondTick() override;

protected:
	FWindow* GameWindow;
	FTextWidget* TextFPSWidget;

	FGameModeManager* GameModeManager;
	FProjectGameMode* GameMode;

	// @TODO Should be moved to thread or class properly
	crow::SimpleApp CrowApp;
};
