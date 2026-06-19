// Project.cpp : Defines the entry point for the application.

#include "ProjectEngine.h"
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
    
    return 0;
}
