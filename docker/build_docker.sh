#!/bin/bash
# build.sh → chmod +x build.sh && ./build.sh

docker build --pull -t squirrelcommunicator:latest .
