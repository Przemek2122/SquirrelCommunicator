// Sentry crash-reporting integration (sentry-native).
//
// This is a thin, dependency-injected wrapper so the rest of the codebase does
// not need to include <sentry.h> directly.  It keeps the integration isolated
// behind a single initialize/shutdown lifecycle pair.

#pragma once

namespace SentryIntegration
{
    /**
     * Initialize sentry-native crash reporting.
     *
     * This is a no-op (with a logged notice) when:
     *   - the build was configured with SQRLL_ENABLE_SENTRY=OFF (SQRLL_HAS_SENTRY
     *     is not defined), or
     *   - the SENTRY_DSN environment variable is missing or empty.
     *
     * The release identifier is compiled into the binary as SQRLL_VERSION so it
     * always matches the debug symbols uploaded by the CI workflow.
     */
    void Initialize();

    /**
     * Flush any pending events and gracefully shut down sentry-native.
     * Safe to call even when Sentry was never initialized.
     */
    void Shutdown();
}
