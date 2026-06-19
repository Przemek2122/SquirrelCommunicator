#!/bin/bash

# Check if either .env.backend OR .env.voice is missing
if [ ! -f .env.backend ] || [ ! -f .env.voice ]; then
    echo "======================================================="
    echo " ERROR: Missing configuration files!"
    echo " Please ensure BOTH files exist in this directory:"
    echo "  - .env.backend"
    echo "  - .env.voice"
    echo " Check the docker/README.md for templates."
    echo "======================================================="
    exit 1
fi

# Check if ports are available
PORTS_TO_CHECK=(8080 8081 8082)

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

# Detect command as it can be 'docker compose' or 'docker-compose' in older versions
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

echo "Configuration files found. Starting container build..."

# Build and start (soft — uses cache, no CACHE_BUST)
$DOCKER_COMPOSE up --build -d

echo "======================================================="
echo "Waiting for backend to start..."
echo "======================================================="

sleep 3

# Test
curl -v http://localhost:8080/health

# Show logs
$DOCKER_COMPOSE logs backend

echo "======================================================="
echo "Waiting for voice_service to start..."
echo "======================================================="

sleep 2

# Test
curl -v http://localhost:8082/health

# Show logs
$DOCKER_COMPOSE logs voice_service
