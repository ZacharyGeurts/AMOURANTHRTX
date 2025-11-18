#!/bin/bash
# linux.sh — AMOURANTH RTX Linux builder — NOW IMMUNE TO CMAKE LOOP

set -e

BUILD_DIR="build"
BIN_DIR="bin/Linux"
CCACHE=$(command -v ccache || true)  # Check if ccache is installed
NINJA=$(command -v ninja || true)    # Check if Ninja is available
GLSLC=$(command -v glslc)             # Check if glslc is installed
GPP14=$(command -v g++-14)            # Check if g++-14 is available
VULKAN_SDK="${VULKAN_SDK:-/usr/include/vulkan}"  # Replace with actual Vulkan SDK path

# Help function to display usage
usage() {
    echo "Usage: ./linux.sh [options]"
    echo "Options:"
    echo "  clean         Clean build directory and shader SPV files."
    echo "  clear         Clean build directory, clear the screen, and start the build."
    echo "  rebuild       Clean and build the project."
    echo "  run           Build the project and immediately run the binary."
    echo "  debug         Build in Debug mode."
    echo "  release       Build in Release mode (default)."
    echo "  --help        Display this help message."
    echo "  -v            Enable verbose mode."
}

# Error message in red
error() {
    echo -e "\033[31mERROR: $1\033[0m"  # Red color
}

# Sanity checks
sanity_check() {
    if [ -z "$GLSLC" ]; then
        error "glslc is not installed. Please install Vulkan SDK."
        exit 1
    fi

    if [ -z "$GPP14" ]; then
        error "g++-14 is not installed."
        exit 1
    fi

    if [ ! -d "$VULKAN_SDK" ]; then
        error "Vulkan SDK is not present at $VULKAN_SDK."
        exit 1
    fi
}

sanity_check

# Clean the build directory and shaders
clean_build() {
    echo "Cleaning Linux build directory and shader SPV files..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
    find shaders -name "*.spv" -delete 2>/dev/null || true
    echo "Clean complete — fresh start guaranteed"
}

# Automatic shader recompilation
recompile_shaders() {
    echo "Recompiling shaders..."
    for shader in shaders/*.frag.glsl shaders/*.vert.glsl; do
        if [ -f "$shader" ]; then
            current_hash=$(sha256sum "$shader" | awk '{ print $1 }')
            shader_name=$(basename "$shader" .glsl)

            if [ -f "shaders/$shader_name.spv" ]; then
                previous_hash=$(cat "shaders/$shader_name.hash" || echo "")
                if [ "$current_hash" == "$previous_hash" ]; then
                    echo "Shader $shader_name.spv is up to date."
                    continue
                fi
            fi

            glslc "$shader" -o "shaders/$shader_name.spv"
            echo "$current_hash" > "shaders/$shader_name.hash"
        fi
    done
}

# Handle command line arguments
BUILD_TYPE="Release"
VERBOSE=false

if [[ "$1" == "clean" || "$1" == "Clean" || "$1" == "CLEAN" ]]; then
    clean_build
elif [[ "$1" == "clear" || "$1" == "Clear" || "$1" == "CLEAR" ]]; then
    clear
    clean_build
elif [[ "$1" == "rebuild" ]]; then
    echo "Cleaning build directory..."
    clean_build
fi

if [[ "$1" == "--help" ]]; then
    usage
    exit 0
elif [[ "$1" == "run" ]]; then
    RUN=true
elif [[ "$1" == "debug" ]]; then
    BUILD_TYPE="Debug"
elif [[ "$1" == "release" ]]; then
    BUILD_TYPE="Release"
elif [[ "$1" == "-v" ]]; then
    VERBOSE=true
fi

# Fresh configure if the build directory does not exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Configuring CMake for Linux..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    if [ -n "$NINJA" ]; then
        BUILD_SYSTEM="-G Ninja"
    else
        BUILD_SYSTEM="-G Unix Makefiles"
    fi

    cmake .. $BUILD_SYSTEM -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=$BUILD_TYPE
else
    echo "Configuring CMake for 
Linux (incremental configure)..."
    cd "$BUILD_DIR"
fi

# Incremental build using ccache if available
if [ -n "$CCACHE" ]; then
    export CC="ccache g++-14"
    export CXX="ccache g++-14"
fi

# Recompile shaders before building if necessary
recompile_shaders

echo "Building AMOURANTH RTX with all cores..."
if [ "$VERBOSE" = true ]; then
    cmake --build . -j$(nproc)
else
    cmake --build . -j$(nproc) > /dev/null
fi

# Run the binary if requested
if [[ "$RUN" == true ]]; then
    echo "Running the built application..."
    ./"$BIN_DIR"/your_executable_name  # Change 'your_executable_name' to your actual binary
fi


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
echo -e "══════════════════════════════════════════════════════════"
echo -e "       Amouranth RTX — Linux build successful ✓"
echo -e "       Binary location: $BUILD_DIR/$BIN_DIR/Navigator"
echo -e "       Run with: cd $BUILD_DIR/$BIN_DIR && ./Navigator"
echo -e "══════════════════════════════════════════════════════════\n"