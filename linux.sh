#!/bin/bash
# linux.sh — AMOURANTH RTX Linux builder — NOW IMMUNE TO CMAKE LOOP

set -e

BUILD_DIR="build"
BIN_DIR="bin/Linux"

# Always nuke build dir on clean — prevents the compiler loop forever
if [ "$1" == "clean" ] || [ "$1" == "Clean" ] || [ "$1" == "CLEAN" ]; then
    echo "Cleaning Linux build directory and shader SPV files..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
    find shaders -name "*.spv" -delete 2>/dev/null || true
    echo "Clean complete — fresh start guaranteed"
fi

# Fresh configure every time — eliminates the restart loop permanently
echo "Configuring CMake for Linux (fresh configure)..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Force g++-14 from the start via command line — bypasses the whole loop
cmake .. -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=Release

echo "Building AMOURANTH RTX with all cores..."
cmake --build . -j$(nproc)

echo ""
echo " "
echo "               █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗"
echo "              ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║"
echo "              ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║"
echo "              ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║"
echo "              ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║"
echo "              ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝"
echo " "
echo "               ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗"
echo "               ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝"
echo "               ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  "
echo "               ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  "
echo "               ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗"
echo "               ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝"
echo " "
echo "        ██████████████████████████████████████████████████████████████████████████████████████"
echo " "
echo "══════════════════════════════════════════════════════════"
echo "       AMOURANTH RTX — LINUX BUILD SUCCESSFUL"
echo "       Binary: build/bin/Linux/Navigator"
echo "       Run:   cd build/bin/Linux && ./Navigator"
echo "══════════════════════════════════════════════════════════"
echo " "