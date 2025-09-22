// Project.cpp : Defines the entry point for the application.

#include "Project.h"

#if !defined(FLAGS_TESTING_PROJECT)
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "Core/CoreEngine.h"
#include "ProjectEngine.h"

struct AppContext 
{
    FEngineManager EngineManager;

    bool hasAppQuit = false;
};

//#if !PLATFORM_ANDROID
int main(int argc, char* argv[])
{
	// Works, can be used to quickly test if it launches and is linked correctly
	/*
    (void)argc;
    (void)argv;
	
    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Hello World",
                                 "!! Your SDL project successfully runs on Android !!", NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShowSimpleMessageBox failed (%s)", SDL_GetError());
        return 1;
    }

    SDL_Quit();
	*/
	
    FEngineManager EngineManager;
    EngineManager.EngineClass.Set<FProjectEngine>();
    EngineManager.Start(argc, argv);
	
    return 0;
}
//#endif

int SDL_Fail()
{
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());

    return -1;
}

int SDL_AppInit(void** AppState, int argc, char* argv[])
{
    // set up the application data
    AppContext* NewAppState = new AppContext
    {
        FEngineManager()
    };

    *AppState = static_cast<void*>(NewAppState);

    // Set class for engine of project
    NewAppState->EngineManager.EngineClass.Set<FProjectEngine>();

    // Start init
    NewAppState->EngineManager.Init(argc, argv);

    SDL_Log("Application started successfully!");

    return 0;
}

int SDL_AppEvent(void *AppState, const SDL_Event* event)
{
	AppContext* Context = static_cast<AppContext*>(AppState);
    
    if (event->type == SDL_EVENT_QUIT)
	{
        Context->hasAppQuit = true;
    }

    return 0;
}

int SDL_AppIterate(void *AppState)
{
	AppContext* Context = static_cast<AppContext*>(AppState);

    Context->EngineManager.LoopIterate();

    return Context->hasAppQuit;
}

void SDL_AppQuit(void* AppState)
{
    AppContext* Context = static_cast<AppContext*>(AppState);
    if (Context != nullptr)
	{
        Context->EngineManager.Exit();

        delete Context;
    }

    SDL_Log("Application quit successfully!");
}
#endif
