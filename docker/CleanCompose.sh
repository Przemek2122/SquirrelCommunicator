#!/bin/bash

# Detect command as it can be 'docker compose' or 'docker-compose' in older version
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "Błąd: Docker Compose nie jest zainstalowany."
    exit 1
fi

# Stop and remove containers, networks, volumes
$DOCKER_COMPOSE down -v

# Remove images built by compose
$DOCKER_COMPOSE down --rmi all
