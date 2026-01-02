#!/bin/bash
# build.sh → chmod +x build.sh && ./build.sh

docker build --no-cache --pull -t  squirrelcommunicator:latest .
