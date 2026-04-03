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

echo "Configuration files found. Starting container build..."

# Build and start
docker-compose up --build -d

echo "Waiting for backend to start..."
sleep 3 # We do not any wait but let's avoid spam for user

# Test
curl -v http://localhost:8080/health

# Show logs
docker-compose logs backend

echo "Waiting for voice_service to start..."
sleep 2 # We do not any wait but let's avoid spam for user

# Test
curl -v http://localhost:8082/health

# Show logs
docker-compose logs voice_service
