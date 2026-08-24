#!/bin/bash

# View streaming logs for the image service only.

# --- Docker Compose detection -------------------------------------------------
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose --env-file .env.backend"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose --env-file .env.backend"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

echo "=== Streaming logs for image_service ==="
$DOCKER_COMPOSE logs -f --tail 100 -t image_service
