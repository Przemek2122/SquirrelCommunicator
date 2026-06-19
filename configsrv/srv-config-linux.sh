#!/bin/bash
set -e

ARCH=${1:-x64}
INITIAL_SAVED_DIR=$(pwd)

# Create build dir and switch to it
cd ..
mkdir -p buildsrv/linux-$ARCH
cd buildsrv/linux-$ARCH

# Use CMAKE to generate ProjectServer (standalone — no Engine)
cmake -G "Unix Makefiles" ../../ProjectServer

# Build the server binary
echo "Building communicatorsrv..."
cmake --build . --target communicatorsrv --parallel
echo "Build complete!"

cd "$INITIAL_SAVED_DIR"

if [ "$CI" = "true" ]; then
echo "Running in CI - skipping IDE open and pause"
else
# Open IDE if available (optional)
echo "Build complete. You can open your IDE manually."
read -p "Press enter to continue..."
fi
