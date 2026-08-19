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
     * A logger callback is installed so sentry-native's internal diagnostics
     * (handler lookup, transport errors, ...) are routed into the application
     * log prefixed with "Sentry:". This is the primary way to confirm the
     * crashpad_handler was found and that events are being uploaded.
     *
     * Crash reporting is made robust against Docker restarts:
     *   - Stale crashpad <uuid>.lock files (left by a handler SIGKILLed mid-upload)
     *     are removed before a new handler is spawned, so pending minidumps from a
     *     previous crash can still be uploaded on the next start.
     *   - The crashpad backend is told to wait for the upload to finish before the
     *     process exits, keeping the container alive long enough for the handler
     *     to complete the upload.
     *
     * The database path is configurable via SENTRY_DB_PATH (default ".sentry-native"
     * in the working directory).
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
