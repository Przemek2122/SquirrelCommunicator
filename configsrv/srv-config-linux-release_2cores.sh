#!/bin/bash
set -e

ARCH=${1:-x64}
INITIAL_SAVED_DIR=$(pwd)

# Create build dir and switch to it
cd ..
mkdir -p buildsrv/linux-$ARCH
cd buildsrv/linux-$ARCH

# Use CMAKE to generate ProjectServer (standalone — no Engine)
# RelWithDebInfo keeps optimized code AND debug symbols for Sentry symbolication.
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo ../../ProjectServer

# Build the server binary
echo "Building communicatorsrv..."
cmake --build . --target communicatorsrv --parallel 2
echo "Build complete!"

cd "$INITIAL_SAVED_DIR"
