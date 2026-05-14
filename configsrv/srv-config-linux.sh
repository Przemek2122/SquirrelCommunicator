#!/bin/bash
set -e

ARCH=${1:-x64}
INITIAL_SAVED_DIR=$(pwd)

# Create build dir and switch to it
cd ..
mkdir -p buildsrv/linux-$ARCH
cd buildsrv/linux-$ARCH

# Use CMAKE to generate ProjectServer
cmake -G "Unix Makefiles" ../../ProjectServer

cd "$INITIAL_SAVED_DIR"
cd ..

# Prebuild every engine ProjectServer so user can skip this.
echo "Try to build all necesary engine projects"
cmake --build buildsrv/linux-$ARCH --target BuildAllEngine --parallel
echo "All engine builds complete!"

# Prebuild every ProjectServer subprojects so user can skip this.
echo "Try to build all necesary projects"
cmake --build buildsrv/linux-$ARCH --target BuildAllProject --parallel
echo "All builds complete!"

if [ "$CI" = "true" ]; then
echo "Running in CI - skipping IDE open and pause"
else
# Open IDE if available (optional)
echo "Build complete. You can open your IDE manually."
read -p "Press enter to continue..."
fi
