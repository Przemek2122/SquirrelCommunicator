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

# --- Port availability checks -------------------------------------------------
PORTS_TO_CHECK=(8080 8081 8082 8083)

echo "Checking port availability..."

for port in "${PORTS_TO_CHECK[@]}"; do
    if ss -tuln | grep -q ":$port "; then
        echo "======================================================="
        echo " ERROR: Port $port is already in use!"
        echo " Please stop the application occupying this port."
        echo " Tip: Use 'sudo ss -tulpn | grep :$port' to find the PID."
        echo "======================================================="
        exit 1
    fi
done

echo "All ports are free. Proceeding..."

# --- Docker Compose detection -------------------------------------------------
# Detect command as it can be 'docker compose' or 'docker-compose' in older versions
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose --env-file .env.backend"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose --env-file .env.backend"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

echo "Configuration files found. Starting container build..."

# Build and start
$DOCKER_COMPOSE build backend
$DOCKER_COMPOSE up --build -d

# --- Service checks (backend -> voice_service -> image_service) ---------------
echo "======================================================="
echo "Waiting for backend to start..."
echo "======================================================="

sleep 2

# Test
curl -v http://localhost:8080/health

# Show logs
$DOCKER_COMPOSE logs backend

echo "======================================================="
echo "Waiting for voice_service to start..."
echo "======================================================="

sleep 1

# Test
curl -v http://localhost:8082/health

# Show logs
$DOCKER_COMPOSE logs voice_service

echo "======================================================="
echo "Waiting for image_service to start..."
echo "======================================================="

sleep 1

# Test
curl -v http://localhost:8083/health

# Show logs
$DOCKER_COMPOSE logs image_service
