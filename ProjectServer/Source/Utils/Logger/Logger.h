#pragma once
#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <condition_variable>

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    ~Logger() {
        bRunning = false;
        queueCV.notify_all();
        if (fileWriterThread.joinable()) {
            fileWriterThread.request_stop();
            fileWriterThread.join();
        }
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    void Log(const std::string& level, const int levelNum, const std::string& message, const std::string& color = "\033[0m") {
#if !DEBUG
        if (VerboseLevel >= levelNum)
#endif
        {
            std::string timestamp = GetTimestamp();
            std::string fullMessage = timestamp + " [" + level + "] " + message;
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << color << fullMessage << "\033[0m" << std::endl;
            }
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                messageQueue.push(fullMessage);
            }
            queueCV.notify_one();
        }
    }

    /// Set verbosity level.
    ///   0 = ERROR only
    ///   1 = ERROR + WARN  (default)
    ///   2 = ERROR + WARN + INFO
    ///   3 = ERROR + WARN + INFO + VERBOSE
    ///
    /// Called once during init — not synchronized (safe: no concurrent readers yet).
    static void SetVerboseLevel(const int Level) { VerboseLevel = Level; }

    /// Static verbosity level accessed directly by logging macros for zero-overhead check.
    /// Default: 1 (shows WARN and ERROR).
    inline static int VerboseLevel = 1;

private:
    Logger() : bRunning(true) {
        logFile.open("communicator.log", std::ios::app);
        fileWriterThread = std::jthread([this](std::stop_token stoken) {
            FileWriterLoop(stoken);
        });
    }

    void FileWriterLoop(std::stop_token stoken) {
        while (!stoken.stop_requested() && bRunning) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !messageQueue.empty() || !bRunning;
            });
            while (!messageQueue.empty()) {
                if (logFile.is_open()) {
                    logFile << messageQueue.front() << std::endl;
                }
                messageQueue.pop();
            }
            if (logFile.is_open()) {
                logFile.flush();
            }
        }
        while (!messageQueue.empty()) {
            if (logFile.is_open()) {
                logFile << messageQueue.front() << std::endl;
            }
            messageQueue.pop();
        }
    }

    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&time, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S]");
        return oss.str();
    }

    std::jthread fileWriterThread;
    std::ofstream logFile;
    std::queue<std::string> messageQueue;
    std::mutex consoleMutex;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> bRunning;
};

// ============================================================================
// Logging macros
// ============================================================================
//
// Level thresholds (non-DEBUG builds):
//   ERROR   — VerboseLevel >= 0  (always fires in practice)
//   WARN    — VerboseLevel >= 1  (DEFAULT)
//   INFO    — VerboseLevel >= 2
//   VERBOSE — VerboseLevel >= 3
//
// DEBUG builds override ALL macros to fire unconditionally, and LOG_DEBUG is
// available. This guarantees you see every log during development regardless
// of the VerboseLoggingLevel config value.
//
// WARNING: INFO and VERBOSE perform JSON parsing and string formatting on
// every WebSocket message. Levels >= 2 cause SIGNIFICANT CPU overhead under
// heavy traffic. Never use level 2+ in production unless diagnosing a
// specific issue.
// ============================================================================

#define LOG_ERROR(msg)   { std::ostringstream oss; oss << msg; Logger::Instance().Log("ERROR", 0,   oss.str(), "\033[31m"); }
#define LOG_WARN(msg)    { std::ostringstream oss; oss << msg; Logger::Instance().Log("WARN", 1,    oss.str(), "\033[33m"); }
#define LOG_INFO(msg)    { std::ostringstream oss; oss << msg; Logger::Instance().Log("INFO", 2,    oss.str(), "\033[0m");  }
#define LOG_VERBOSE(msg) { std::ostringstream oss; oss << msg; Logger::Instance().Log("VERBOSE", 3, oss.str(), "\033[36m"); }

#if DEBUG
#define LOG_DEBUG(msg)   { std::ostringstream oss; oss << msg; Logger::Instance().Log("DEBUG", 0,   oss.str(), "\033[90m"); }
#else
#define LOG_DEBUG(msg)   { }
#endif
