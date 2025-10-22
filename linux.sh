#!/bin/bash
# linux.sh
# Builds AMOURANTH RTX for Linux natively
# Usage: ./linux.sh [clean]

set -e

# Define directories
BUILD_DIR="build"
BIN_DIR="bin/Linux"
SHADER_DIR="shaders/raytracing"
SHADER_RM_DIR="shaders"
RASTERIZATION_SHADER_DIR="shaders/rasterization"

# Clean build directory and shader SPV files if 'clean' is passed
if [ "$1" == "clean" ]; then
    echo "Cleaning Linux build directory and shader SPV files..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
    rm -f "$SHADER_RM_DIR"/*.spv
    rm -f "$RASTERIZATION_SHADER_DIR"/*.spv
fi

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake and build
echo "Configuring CMake for Linux..."
cmake ..
echo "Building AMOURANTH RTX for Linux..."
cmake --build . -j $(nproc)

# Verify output
if [ -f "../$BIN_DIR/Navigator" ]; then
    echo "Linux build successful! Binary located at ../$BIN_DIR/Navigator"
else
    echo "Linux build finished! Check for errors above."
    exit 1
fi

echo "Linux build completed."