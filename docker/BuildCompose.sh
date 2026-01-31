#!/bin/bash

# Build and start
docker-compose up --build -d

# Wait for service to be ready
echo "Waiting for mediasoup to start..."
sleep 3

# Test
curl http://localhost:3000/health

# Show logs
docker-compose logs mediasoup
