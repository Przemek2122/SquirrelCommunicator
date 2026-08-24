#!/bin/bash

# View streaming logs for ALL services (backend, voice_service, image_service).

# --- Docker Compose detection -------------------------------------------------
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose --env-file .env.backend"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose --env-file .env.backend"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

echo "=== Streaming logs for ALL services (backend, voice_service, image_service) ==="
$DOCKER_COMPOSE logs -f --tail 100 -t backend voice_service image_service
