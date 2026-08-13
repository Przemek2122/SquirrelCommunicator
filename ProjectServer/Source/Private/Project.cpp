// Project.cpp : Defines the entry point for the application.

#include "ProjectEngine.h"
#include "Sentry/SentryIntegration.h"
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

std::atomic<bool> GIsProjectRunning{ true };

void signalHandler(int signal) {
    GIsProjectRunning = false;
}

int main(int argc, char* argv[])
{
    // Crash reporting must start before any engine code so crashes during
    // initialization are captured as well.  No-op when SENTRY_DSN is unset.
    SentryIntegration::Initialize();

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    FProjectEngine Engine;
    Engine.Init();

    while (GIsProjectRunning)
    {
        Engine.PostSecondTick();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Engine.PreExit();

    // Flush and gracefully shut down crash reporting before exit.
    SentryIntegration::Shutdown();

    return 0;
}
