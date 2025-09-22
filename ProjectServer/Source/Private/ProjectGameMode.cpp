#include "ProjectGameMode.h"
#include "ProjectEngine.h"

FProjectGameMode::FProjectGameMode(FGameModeManager* InGameModeManager)
	: FGameModeBase(InGameModeManager)
{
}

void FProjectGameMode::Start()
{
	FGameModeBase::Start();
}

void FProjectGameMode::End()
{
	FGameModeBase::End();
}

