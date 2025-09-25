#include "ProjectEngine.h"
#include "UserManager.h"
#include "Assets/IniReader/IniManager.h"
#include "Assets/IniReader/IniObject.h"
#include "Misc/EncryptionManager.h"
#include "Misc/PasswordEncryptionBase.h"
#include "Threads/ThreadsManager.h"

FProjectEngine::FProjectEngine()
	: UserManager(new FUserManager())
{
}

void FProjectEngine::Init()
{
	FEngine::Init();

	LOG_DEBUG("Server init");

	FIniManager* IniManager = FGlobalDefines::GEngine->GetAssetsManager()->GetIniManager();
	std::shared_ptr<FIniObject> ServerSettingsIni = IniManager->GetIniObject("ServerSettings");
	if (ServerSettingsIni->DoesIniExist())
	{
		ServerSettingsIni->LoadIni();

		// Most common address to check if it works
		CROW_ROUTE(CrowApp, "/")([]()
		{
			return "Crow C++ API Server is running.";
		});

		// Route for testing if api works
		CROW_ROUTE(CrowApp, "/api/test")([]()
		{
			return "true";
		});

		LOG_DEBUG("Created api test");

		CROW_ROUTE(CrowApp, "/api/user")([]()
		{
			return "no user specified";
		});

		LOG_DEBUG("Created api user");

		// Find port in settings
		constexpr uint16 ServerPortDefault = 8080;
		int32 ServerPort;
		if (ServerSettingsIni)
		{
			const FIniField ServerPortField = ServerSettingsIni->FindFieldByName("Port");
			ServerPort = ServerPortField.GetValueAsInt();
		}
		else
		{
			ServerPort = ServerPortDefault;
		}

		FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
		FThreadData* CrowThreadData = ThreadsManager->CreateThread<FGenericThread, FThreadData>("CrowThread");
		FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(CrowThreadData->GetThread());
		if (GenericThread != nullptr)
		{
			GenericThread->AddTask([this, ServerPort]()
			{
				CrowApp.port(static_cast<uint16>(ServerPort)).multithreaded().run();
			});

			LOG_DEBUG("Started server at port: '" << ServerPort << "'.");
			LOG_DEBUG("Go to localhost:" << ServerPort << "\\");
		}
	}
	else
	{
		LOG_ERROR("Ini is missing, API will not work.");
	}

	/*
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
	*/
}

void FProjectEngine::PostSecondTick()
{
	FEngine::PostSecondTick();

	UserManager->PostSecondTick();
}
