// Sentry crash-reporting integration (sentry-native).

#include "Sentry/SentryIntegration.h"
#include "Logger/Logger.h"

#ifdef SQRLL_HAS_SENTRY
    #include <sentry.h>
    #include <cstdlib>
    #include <filesystem>
    #include <string>

    namespace
    {
        // Routes sentry-native's internal logging into the application logger.
        //
        // This is what makes failures visible: without it, problems such as a
        // missing crashpad_handler, an invalid DSN, or transport/network errors
        // are written to stderr and effectively lost.  Note that the Crashpad
        // backend can return 0 from sentry_init() even when it failed to spawn
        // the handler, so these log lines are often the only clue that crashes
        // are being dropped.
        void SentryLogger(sentry_level_t Level, const char* Message, void* /*UserData*/)
        {
            if (Message == nullptr || Message[0] == '\0')
            {
                return;
            }

            switch (Level)
            {
                case SENTRY_LEVEL_FATAL:
                case SENTRY_LEVEL_ERROR:
                    LOG_ERROR("Sentry: " << Message);
                    break;
                case SENTRY_LEVEL_WARNING:
                    LOG_WARN("Sentry: " << Message);
                    break;
                case SENTRY_LEVEL_INFO:
                    LOG_INFO("Sentry: " << Message);
                    break;
                case SENTRY_LEVEL_DEBUG:
                default:
                    LOG_DEBUG("Sentry: " << Message);
                    break;
            }
        }

        // Removes stale per-report lock files left behind by a previous
        // crashpad_handler process that was killed before it could finish.
        //
        // crashpad guards every report in <db>/pending (and new/completed) with
        // a <uuid>.lock file created with O_CREAT|O_EXCL. When a crash happens
        // inside a Docker container the server (PID 1) exits, the container is
        // stopped, and the crashpad_handler child process is SIGKILLed mid-upload
        // -- leaving those .lock files behind. On the next start the new handler
        // tries to create the same lock, hits EEXIST ("File exists (17)"), and
        // silently skips the report forever, so no crash is ever uploaded.
        //
        // This runs before sentry_init() spawns a new handler, so every .lock
        // file present at that point is stale and safe to remove. We only touch
        // per-report locks inside new/pending/completed; the settings lock in the
        // database root is left to crashpad's own TTL-based handling.
        void CleanStaleCrashpadLocks(const std::string& DatabasePath)
        {
            size_t Removed = 0;

            for (const char* Subdir : { "new", "pending", "completed" })
            {
                const std::filesystem::path Dir = std::filesystem::path(DatabasePath) / Subdir;
                try
                {
                    if (!std::filesystem::is_directory(Dir))
                    {
                        continue;
                    }

                    for (const auto& Entry : std::filesystem::directory_iterator(Dir))
                    {
                        if (Entry.path().extension() == ".lock")
                        {
                            std::error_code RemoveEc;
                            std::filesystem::remove(Entry.path(), RemoveEc);
                            if (!RemoveEc)
                            {
                                LOG_DEBUG("Sentry: removed stale crashpad lock " << Entry.path().string());
                                ++Removed;
                            }
                        }
                    }
                }
                catch (const std::filesystem::filesystem_error&)
                {
                    // Database directory does not exist yet or is unreadable;
                    // there is nothing to clean up.
                }
            }

            if (Removed > 0)
            {
                LOG_INFO("Sentry: removed " << Removed << " stale crashpad lock file(s).");
            }
        }
    }
#endif

void SentryIntegration::Initialize()
{
#ifdef SQRLL_HAS_SENTRY
    // The DSN is injected via environment (docker/.env.backend) rather than
    // hardcoded in source, so it is never committed to the repository.
    const char* Dsn = std::getenv("SENTRY_DSN");
    if (Dsn == nullptr || Dsn[0] == '\0')
    {
        LOG_WARN("SENTRY_DSN is not set - Sentry crash reporting is disabled.");
        return;
    }

    // Crashpad database directory. This persists across restarts so minidumps
    // from a previous crash can be uploaded on the next start. Defaults to
    // ".sentry-native" resolved against the working directory (/app in Docker).
    const char* DbPathEnv = std::getenv("SENTRY_DB_PATH");
    const std::string DatabasePath =
        (DbPathEnv != nullptr && DbPathEnv[0] != '\0') ? DbPathEnv : ".sentry-native";

    // Drop stale .lock files left by a previously killed handler before a new
    // one is spawned, otherwise pending reports can never be uploaded.
    CleanStaleCrashpadLocks(DatabasePath);

    // Environment (development / staging / production), overridable at runtime.
    const char* EnvValue = std::getenv("SENTRY_ENVIRONMENT");
    const std::string Environment =
        (EnvValue != nullptr && EnvValue[0] != '\0') ? EnvValue : "production";

    sentry_options_t* Options = sentry_options_new();

    // Surface sentry-native diagnostics through the app logger so handler /
    // transport failures are visible instead of silently dropping crashes.
    sentry_options_set_logger(Options, SentryLogger, nullptr);

    sentry_options_set_dsn(Options, Dsn);
    sentry_options_set_environment(Options, Environment.c_str());

    // The release must match the value uploaded by CI (SQRLL_VERSION is the git
    // commit hash extracted at CMake configure time and compiled into this TU).
    sentry_options_set_release(Options, SQRLL_VERSION);

    sentry_options_set_database_path(Options, DatabasePath.c_str());

    // Wait for the crash upload to finish before the process exits. In Docker
    // the server runs as PID 1: when it exits the whole container is stopped and
    // the crashpad_handler child is SIGKILLed, losing the report. Blocking the
    // crashing thread until the upload completes keeps the container alive long
    // enough for the handler to finish (crashpad only, Linux/macOS/Windows).
    sentry_options_set_crashpad_wait_for_upload(Options, 1);

#if DEBUG
    // Emit sentry-native's own debug output while developing. This shows the
    // crashpad_handler lookup path and transport activity, which makes it easy
    // to confirm the handler was found and events are being uploaded.
    sentry_options_set_debug(Options, 1);
#endif

    // Crashpad backend: by default sentry looks for the crashpad_handler
    // executable in the same directory as the running binary.  CMake copies it
    // next to communicatorsrv as a post-build step.
    if (sentry_init(Options) != 0)
    {
        LOG_ERROR("Sentry initialization failed.");
        return;
    }

    LOG_STATE("Sentry initialized (release: " << SQRLL_VERSION
              << ", environment: " << Environment
              << ", database: " << DatabasePath
              << ", dsn: set)");
#else
    LOG_DEBUG("Sentry is disabled at build time (SQRLL_ENABLE_SENTRY=OFF).");
#endif
}

void SentryIntegration::Shutdown()
{
#ifdef SQRLL_HAS_SENTRY
    sentry_close();
#endif
}
