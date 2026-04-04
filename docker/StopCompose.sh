#!/bin/bash

# Detect command
if docker compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker compose"
elif docker-compose version >/dev/null 2>&1; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "Błąd: Docker Compose nie jest zainstalowany."
    exit 1
fi

$DOCKER_COMPOSE down

$DOCKER_COMPOSE ps

