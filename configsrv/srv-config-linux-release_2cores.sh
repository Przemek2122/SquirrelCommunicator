#!/bin/bash

ARCH=${1:-x64}
INITIAL_SAVED_DIR=$(pwd)

# Create build dir and switch to it
cd ..
mkdir -p buildsrv/linux-$ARCH
cd buildsrv/linux-$ARCH

# Use CMAKE to generate ProjectServer
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../../ProjectServer

cd "$INITIAL_SAVED_DIR"
cd ..

# Prebuild every engine ProjectServer so user can skip this.
echo "Try to build all necessary engine projects"
cmake --build buildsrv/linux-$ARCH --target BuildAllEngine --parallel 2
echo "All engine builds complete!"

# Prebuild every ProjectServer subprojects so user can skip this.
echo "Try to build all necessary projects"
cmake --build buildsrv/linux-$ARCH --target BuildAllProject --parallel 2
echo "All builds complete!"

