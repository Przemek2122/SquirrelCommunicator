# Docker and `.env` File Configuration

This document explains how to properly configure the environment variables required to run the connected services:
1. **C++ Backend** (REST CROWCPP & UWebSockets)
2. **Go Voice Service** (SquirrelCommunicatorVoice)

## Dockerfiles

| File | Purpose | Build time |
|---|---|---|
| `Dockerfile.release` | Downloads pre-built binary from GitHub Releases (recommended) | ~10 seconds |
| `Dockerfile` | Compiles from source inside container (for development/debugging) | ~10 minutes |

`docker-compose.yml` uses `Dockerfile.release` by default.

## Configuration INI (repo vs. baked-in)

The backend reads its runtime settings from `Assets/Config/BackendSettings.ini`.

The pre-built release tarball ships with a **snapshot** of that file baked in at
release-build time. To avoid running with stale settings, **both** Dockerfiles
replace that snapshot with the **latest config from the repository** at image
build time — regardless of whether the image uses the pre-built GitHub binary
or is compiled locally:

1. `Dockerfile.release` shallow-clones the repository (no submodules) and copies
   `ProjectServer/Assets/Config/BackendSettings.ini` over the baked-in copy.
2. `Dockerfile` already clones the repository to compile from it; it additionally
   copies `ProjectServer/Assets/Config/BackendSettings.ini` from that checkout
   over the build output, so the source is always the repo file, not any
   snapshot embedded elsewhere.

In both cases the copy runs **once per image build** and is cached in the image
layer — there is **no network access at container runtime** and no dependency on
GitHub when the container starts. If the copy fails for any reason, the image
build still succeeds and keeps the previously available config, so the image can
always start.

### Refreshing the config

Config is fetched at **build time**, so to pick up the latest repo config you
rebuild the image. Because Docker caches the fetch layer, force a refresh with:

```bash
docker compose build --no-cache backend
docker compose up -d backend
```

or bump the cache-bust argument (`Dockerfile.release`):

```bash
docker compose build --build-arg CONFIG_CACHE_BUST="$(date +%s)" backend
docker compose up -d backend
```

For the source-build `Dockerfile`, use `CACHE_BUST` instead:

```bash
docker compose build --build-arg CACHE_BUST="$(date +%s)" backend
docker compose up -d backend
```

## Configuration Files

### `.env.backend` (backend container env)

- Database is separate for data safety. It could be added to services but its management would be complicated
- Brevo key can be generated at 'https://app.brevo.com/settings/keys/api'

```
# Version of the backend release to deploy
BACKEND_VERSION=1.0.9

# Database connections env vars
SQRLL_COMM_DB_HOST=127.0.0.1
SQRLL_COMM_DB_PORT=3306
SQRLL_COMM_DB_DBNAME=sqrllapitest
SQRLL_COMM_DB_USER=commapisqrllusertest
SQRLL_COMM_DB_PASSWORD=

# Brevo API key
SQRLL_COMM_MAIL_API_KEY=

# --- Sentry crash reporting (optional but recommended) ---
# Public ingest endpoint for this project. It is safe to keep here (it is also
# embedded in the shipped client binaries), but it is NOT the auth token.
SENTRY_DSN=

# Which Sentry environment this deployment belongs to
# (production / staging / development). Defaults to "production" if unset.
SENTRY_ENVIRONMENT=production
```

### `.env.voice` (voice service container env)

- SQRLL_VOICE_API_KEY - It is backend password to do not allow random calls to create random rooms.

```
# Address for backend app
SQRLL_VOICE_ADDRESS=127.0.0.1

# The port the Go service runs on for Voice and Backend app server
SQRLL_VOICE_PORT=8082

# The main password for communicating with the backend (Change this!)
SQRLL_VOICE_API_KEY=
```

### `.env.image` (voice service container env)

```
PORT=8083
SQRLL_IMAGE_API_KEY=
MAX_DISK_GB=200
MAX_RAM_MB=1024
MAX_UPLOAD_MB=8
```

### `.env.encryptionpass` (Encryption password file)

```
# Message encryption key (used instead of MessageEncryptionKeyFile from ini config).
SQRLL_MESSAGE_ENCRYPTION_KEY=
```

## Sentry Crash Reporting

The backend ships with `sentry-native` (Crashpad backend) compiled in. When a
`SENTRY_DSN` is present, unhandled crashes (`SIGSEGV`, `std::terminate`, `std::bad_alloc`, …)
are captured as minidumps and uploaded to Sentry automatically.

### Two different values — do not confuse them

| Variable | Secret? | Purpose |
|---|---|---|
| `SENTRY_DSN` | Public-ish | Ingest endpoint that tells the running server *where* to send crash events. Set in `.env.backend`. |
| `SENTRY_AUTH_TOKEN` | **PRIVATE** | Used by CI (`sentry-cli`) to *upload debug symbols*. Never put this in `.env` files or source — store it in GitHub Secrets. |

### Runtime variables (container)

Set these in `.env.backend`:

- `SENTRY_DSN` — required to enable crash reporting. If empty, the server runs normally but does not report crashes.
- `SENTRY_ENVIRONMENT` — optional label (`production`, `staging`, `development`). Defaults to `production`.

### CI variables (GitHub Secrets)

Used by `.github/workflows/release.yml` to upload debug symbols so crashes are
symbolicated (stack traces show source lines instead of raw addresses):

- `SENTRY_AUTH_TOKEN` — an auth token from Sentry (Organization → Settings → Auth Tokens).
- `SENTRY_ORG` — your Sentry organization slug.
- `SENTRY_PROJECT` — your Sentry project slug.

If any of these are missing, the release workflow still completes — it just skips
the debug-symbol upload step.

### Disabling Sentry at build time

Local builds that do not need crash reporting (or lack `libcurl`/`zlib` dev headers)
can disable it at CMake configure time:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSQRLL_ENABLE_SENTRY=OFF
```
