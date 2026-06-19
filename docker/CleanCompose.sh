#!/bin/bash

# Detect command
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose --env-file .env.backend"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose --env-file .env.backend"
else
    echo "Error: Docker Compose is not installed."
    exit 1
fi

# Stop and remove containers, networks, volumes
$DOCKER_COMPOSE down -v

# Remove images built by compose
$DOCKER_COMPOSE down --rmi all
