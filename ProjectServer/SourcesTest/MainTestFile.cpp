#include <gtest/gtest.h>
#include "Public/ProjectEngine.h"

int main(int argc, char** argv)
{
    FEngineManager EngineManager;

    std::thread EngineThread([&]()
    {
        EngineManager.EngineClass.Set<FProjectEngine>();
        EngineManager.Start(argc, argv);
    });

    // Wait for EngineThread as we need server for tests
    for (int32 i = 0; i < 1024; ++i)
    {
        FEngine* Engine = FEngineManager::Get();
        if (Engine != nullptr)
        {
            // Engine initialised
            break;
        }
        else
        {
	        // Wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Wait for crow to initialize
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Run test
    ::testing::InitGoogleTest(&argc, argv);
    const int RunOutput = RUN_ALL_TESTS();

    // Shut down engine
    FProjectEngine* ProjectEngine = FEngineManager::Get<FProjectEngine>();
    if (ProjectEngine != nullptr)
    {
        ProjectEngine->RequestExit();
    }
    else
    {
        LOG_ERROR("Engine init failed");
    }

    // Join threads
    EngineThread.join();

    return RunOutput;
}
