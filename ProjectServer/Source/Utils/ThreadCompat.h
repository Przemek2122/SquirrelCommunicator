// Thread compatibility layer - minimal utilities after FThreadsManager removal

#pragma once

#include <thread>
#include <chrono>

// Macro replacement for Engine's THREAD_WAIT_MS
#define THREAD_WAIT_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))

/** Get number of logical CPUs (replaces FThreadsManager::GetNumberOfLogicalCPU) */
inline int32_t GetNumberOfLogicalCPU()
{
	unsigned int cores = std::thread::hardware_concurrency();
	return cores > 0 ? static_cast<int32_t>(cores) : 1;
}
