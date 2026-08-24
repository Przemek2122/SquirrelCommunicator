#!/usr/bin/env bash
# Upload debug symbols to Sentry locally (self-hosted / manual builds).
#
# This mirrors the CI step in .github/workflows/release.yml so crashes can be
# symbolicated when building outside GitHub Actions (where GitHub Secrets are not
# available).
#
# Reads the upload credentials from docker/.env.sentry (gitignored). If any of
# SENTRY_AUTH_TOKEN / SENTRY_ORG / SENTRY_PROJECT are missing, the upload is
# skipped.
#
# Usage:
#   cd docker
#   ./UploadSentrySymbols.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/.env.sentry"

if [[ ! -f "${ENV_FILE}" ]]; then
    echo "Error: ${ENV_FILE} not found. Copy the template from docker/README.md." >&2
    exit 1
fi

# shellcheck disable=SC1090
source "${ENV_FILE}"

if [[ -z "${SENTRY_AUTH_TOKEN:-}" || -z "${SENTRY_ORG:-}" || -z "${SENTRY_PROJECT:-}" ]]; then
    echo "Skipping symbol upload: SENTRY_AUTH_TOKEN / SENTRY_ORG / SENTRY_PROJECT not set in ${ENV_FILE}." >&2
    exit 0
fi

# The release identifier is the git commit hash — the exact value that was
# compiled into the binary via CMake (SQRLL_VERSION). This keeps the uploaded
# debug symbols matched to the exact build that ships.
RELEASE="$(git rev-parse --short HEAD)"

BINARY="${SCRIPT_DIR}/../ProjectServer/build/bin/Release/Project/communicatorsrv"
if [[ ! -f "${BINARY}" ]]; then
    echo "Error: binary not found at ${BINARY}. Build it first." >&2
    exit 1
fi

# Install sentry-cli if it isn't already available.
if ! command -v sentry-cli >/dev/null 2>&1; then
    echo "sentry-cli not found, installing..."
    curl -sL https://sentry.io/get-cli/ | bash
fi

sentry-cli releases new "${RELEASE}"
sentry-cli debug-files upload --include-sources "${BINARY}"
sentry-cli releases finalize "${RELEASE}"

echo "Debug symbols uploaded for release ${RELEASE}."
