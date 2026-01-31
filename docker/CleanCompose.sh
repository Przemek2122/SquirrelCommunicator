#!/bin/bash

# Stop and remove containers, networks, volumes
docker-compose down -v

# Remove images built by compose
docker-compose down --rmi all
