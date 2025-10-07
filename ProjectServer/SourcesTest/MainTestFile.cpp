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

    // Run test
    ::testing::InitGoogleTest(&argc, argv);
    const int RunOutput = RUN_ALL_TESTS();

    FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
    if (ProjectEngine != nullptr)
    {
        LOG_INFO("Testing done.");

        ProjectEngine->RequestExit();


    }
    else
    {
        LOG_ERROR("Missing ProjectEngine");
    }

    // Join threads
    EngineThread.join();

    return RunOutput;
}

TEST(BackendTest, Crow)
{
    FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
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
