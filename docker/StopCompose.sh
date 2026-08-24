#!/bin/bash

# --- Configuration file checks ------------------------------------------------
# All four files are required by docker-compose.yml (backend env_file list).
# .env.sentry is intentionally optional (required: false) and NOT checked here.
REQUIRED_FILES=(.env.backend .env.encryptionpass .env.voice .env.image)

MISSING_FILES=()
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        MISSING_FILES+=("$file")
    fi
done

if [ ${#MISSING_FILES[@]} -gt 0 ]; then
    echo "======================================================="
    echo " ERROR: Missing configuration file(s)!"
    echo " Please ensure ALL of the following exist in this directory:"
    echo "  - .env.backend"
    echo "  - .env.encryptionpass"
    echo "  - .env.voice"
    echo "  - .env.image"
    echo ""
    echo " Missing:"
    for file in "${MISSING_FILES[@]}"; do
        echo "  - $file"
    done
    echo ""
    echo " Check docker/README.md for templates."
    echo "======================================================="
    exit 1
fi

# --- Docker Compose detection -------------------------------------------------
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose --env-file .env.backend"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose --env-file .env.backend"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

$DOCKER_COMPOSE down

$DOCKER_COMPOSE ps
