// Sentry crash-reporting integration (sentry-native).

#include "Sentry/SentryIntegration.h"
#include "Logger/Logger.h"

#ifdef SQRLL_HAS_SENTRY
    #include <sentry.h>
    #include <cstdlib>
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

    sentry_options_t* Options = sentry_options_new();

    sentry_options_set_dsn(Options, Dsn);

    // Environment (development / staging / production), overridable at runtime.
    const char* Environment = std::getenv("SENTRY_ENVIRONMENT");
    sentry_options_set_environment(
        Options,
        (Environment != nullptr && Environment[0] != '\0') ? Environment : "production");

    // The release must match the value uploaded by CI (SQRLL_VERSION is the git
    // commit hash extracted at CMake configure time and compiled into this TU).
    sentry_options_set_release(Options, SQRLL_VERSION);

    // Crashpad backend: by default sentry looks for the crashpad_handler
    // executable in the same directory as the running binary.  CMake copies it
    // next to communicatorsrv as a post-build step.
    if (sentry_init(Options) != 0)
    {
        LOG_ERROR("Sentry initialization failed.");
        return;
    }

    LOG_STATE("Sentry initialized (release: " << SQRLL_VERSION << ")");
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
