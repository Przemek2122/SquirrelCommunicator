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

    // Run test
    ::testing::InitGoogleTest(&argc, argv);
    const int RunOutput = RUN_ALL_TESTS();

    // Shut down engine
    FEngine* Engine = FEngineManager::Get();
    Engine->RequestExit();

    // Join threads
    EngineThread.join();

    return RunOutput;
}

TEST(BackendTest, Crow)
{
    FProjectEngine* ProjectEngine = FEngineManager::Get<FProjectEngine>();
    EXPECT_TRUE(ProjectEngine != nullptr);
    if (ProjectEngine != nullptr)
    {
	    crow::SimpleApp& CrowApp = ProjectEngine->GetCrowApp();

        CrowApp.validate();
    }
}

TEST(BackendTest, SimpleClient)
{
	// Use ASIO for tests?
	// https://crowcpp.org/master/guides/testing/


}
