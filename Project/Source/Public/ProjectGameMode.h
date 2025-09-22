// Created by Przemys³aw Wiewióra 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "Core/CoreMinimal.h"
#include "Engine/Logic/GameModeBase.h"

class FProjectGameMode : public FGameModeBase
{
public:
	FProjectGameMode(FGameModeManager* InGameModeManager);

	void Start() override;
	void End() override;

};
