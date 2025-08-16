#!/bin/bash

# Build script for AfterHours project
# Usage: ./build.sh [debug|release|clean]

set -e

BUILD_DIR="build"
BUILD_TYPE="Debug"

case "${1:-debug}" in
    debug|Debug|DEBUG)
        BUILD_TYPE="Debug"
        echo "Building in Debug mode (profiling enabled)"
        ;;
    release|Release|RELEASE)
        BUILD_TYPE="Release"
        echo "Building in Release mode (profiling disabled)"
        ;;
    clean)
        echo "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        exit 0
        ;;
    *)
        echo "Usage: $0 [debug|release|clean]"
        echo "  debug   - Build with debugging and profiling enabled"
        echo "  release - Build with optimizations and profiling disabled"
        echo "  clean   - Remove build directory"
        exit 1
        ;;
esac

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

# Build
echo "Building..."
cmake --build . --config "$BUILD_TYPE"

echo "Build completed successfully!"
echo "Executable location: $BUILD_DIR/AfterHours"