#include "ProjectEngine.h"
#include "ProjectGameMode.h"
#include "Engine/Logic/GameModeManager.h"
#include "Renderer/Widgets/Samples/ButtonWidget.h"
#include "Renderer/Widgets/Samples/MouseSparkWidget.h"
#include "Renderer/Widgets/Samples/TextWidget.h"
#include "Renderer/Widgets/Samples/VerticalBoxWidget.h"

FProjectEngine::FProjectEngine()
	: GameWindow(nullptr)
	, TextFPSWidget(nullptr)
	, GameModeManager(nullptr)
	, GameMode(nullptr)
{
}

void FProjectEngine::Init()
{
	FEngine::Init();

	LOG_DEBUG("Game init");

	FWindowCreationData WindowCreationData(false);
	const FVector2D<int32> NewWindowLocation = { 200, 200 };
	const FVector2D<int32> NewWindowSize = { 800, 600 };
	GameWindow = GetEngineRender()->CreateWindow<FWindow>(WindowCreationData, "Sample window", NewWindowLocation, NewWindowSize);
	if (GameWindow != nullptr)
	{
		// Create game mode subsystem for window
		GameModeManager = GameWindow->CreateSubSystem<FGameModeManager>(GameWindow);
		GameMode = GameModeManager->CreateGameMode<FProjectGameMode>(true);

        // Get window manager for creating widgets
		FWidgetManager* GameWindowWidgetManager = GameWindow->GetWidgetManager();

        // Add sample 'FMouseSparkWidget' widget
		GameWindowWidgetManager->CreateWidget<FMouseSparkWidget>("TestSparkWidget", 100);
		TextFPSWidget = GameWindowWidgetManager->CreateWidget<FTextWidget>("TextFPS");
		TextFPSWidget->SetText("FPS");
		TextFPSWidget->SetAnchor(EAnchor::RightTop);
		TextFPSWidget->SetTextWidgetSizeType(ETextWidgetSizeType::None);

		FVerticalBoxWidget* VerticalBoxWidget = GameWindow->GetWidgetManager()->CreateWidget<FVerticalBoxWidget>("TestVerticalBoxWidget");
		VerticalBoxWidget->SetAnchor(EAnchor::Center);
		VerticalBoxWidget->SetWidgetSizePercent({ 0.5f, 0.5f }, EWidgetSizeType::ParentPercentage);

		FButtonWidget* StartButtonWidget = VerticalBoxWidget->CreateWidget<FButtonWidget>();
		StartButtonWidget->UseDefaultSize();
		FTextWidget* StartTextWidget = StartButtonWidget->CreateWidget<FTextWidget>();
		StartTextWidget->SetText("Start");
		StartButtonWidget->OnLeftClickPress.BindLambda([this, VerticalBoxWidget]
		{
			LOG_DEBUG("Start requested!");

			GameWindow->DestroyWidget(VerticalBoxWidget);

			//GameMode->StartGame();
		});

		FButtonWidget* ExitButtonWidget = VerticalBoxWidget->CreateWidget<FButtonWidget>();
		ExitButtonWidget->UseDefaultSize();
		FTextWidget* ExitTextWidget = ExitButtonWidget->CreateWidget<FTextWidget>();
		ExitTextWidget->SetText("Exit");
		ExitButtonWidget->OnLeftClickPress.BindLambda([this]
		{
			LOG_DEBUG("Exit requested!");

			RequestExit();
		});
	}
}

void FProjectEngine::Tick()
{
	FEngine::Tick();
}

void FProjectEngine::PostSecondTick()
{
	FEngine::PostSecondTick();

	// Update FPS number every second
	//TextFPSWidget->SetText(std::to_string(FGlobalDefines::GEngine->GetFramesThisSecond()));
}
