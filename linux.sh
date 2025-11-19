#!/bin/bash
# =============================================================================
# linux.sh — AMOURANTH RTX ULTIMATE LINUX BUILD SCRIPT v3.0
# DUAL LICENSED — COMMERCIAL BUILD — BLUE CHECKMARK ETERNAL — PINK PHOTONS FOREVER
# =============================================================================
# → Debug is default
# → Ninja OFF by default (Make = default, safe, predictable)
# → Accepts any combo/order: ./linux.sh release ninja run clean --help
# → Smart shader hashing (only changed .glsl → recompiled)
# → ./linux.sh --help shows empire doctrine
# =============================================================================

set -e

BUILD_DIR="build"
BIN_DIR="build/bin/Linux"
SHADERS_OUT="$BIN_DIR/assets/shaders"
BINARY="Navigator"

# Empire Colors
R='\033[0;31m' G='\033[0;32m' Y='\033[1;33m' B='\033[0;34m' M='\033[0;35m' C='\033[0;36m' W='\033[1;37m' N='\033[0m'

valhalla_banner() {
    clear
    echo -e "${M}"
    cat << "EOF"
          █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗
         ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║
         ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║
         ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║
         ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║
         ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝
EOF
    echo -e "${C}                        VALHALLA v80 TURBO — PINK PHOTONS ETERNAL — NOVEMBER 19 2025${N}"
    echo
}

show_help() {
    valhalla_banner
    echo -e "${W}AMOURANTH RTX BUILD DOCTRINE — ACCEPTED COMMANDS (any order):${N}"
    echo
    echo -e "  ${G}./linux.sh${N}                    → Build Debug (default) with Make"
    echo -e "  ${G}./linux.sh release${N}             → Build Release (O3 + LTO)"
    echo -e "  ${G}./linux.sh ninja${N}               → Use Ninja instead of Make"
    echo -e "  ${G}./linux.sh release ninja${N}       → Release + Ninja"
    echo -e "  ${G}./linux.sh run${N}                 → Build Debug + launch instantly"
    echo -e "  ${G}./linux.sh clean${N}               → Nuclear wipe + fresh start"
    echo -e "  ${G}./linux.sh clear${N}               → Same as clean"
    echo -e "  ${G}./linux.sh --help${N}              → This banner"
    echo
    echo -e "${M}PINK PHOTONS ETERNAL — STONEKEY v∞ ACTIVE — BLUE CHECKMARK SECURE${N}"
    exit 0
}

nuke_empire() {
    valhalla_banner
    echo -e "${R}NUCLEAR PURGE INITIATED — ALL ARTIFACTS ANNIHILATED${N}"
    rm -rf "$BUILD_DIR" "$BIN_DIR" .shader_hash_cache
    echo -e "${G}Empire reborn. Fresh start achieved.${N}"
    exit 0
}

# =============================================================================
# PARSE ARGUMENTS — ANY ORDER ACCEPTED
# =============================================================================

BUILD_TYPE="Debug"      # default
USE_NINJA="no"          # Ninja OFF by default
RUN_AFTER="no"

for arg in "$@"; do
    case "${arg,,}" in
        --help|-h)     show_help ;;
        clean|clear)   nuke_empire ;;
        release)       BUILD_TYPE="Release" ;;
        ninja)         USE_NINJA="yes" ;;
        run)           RUN_AFTER="yes" ;;
    esac
done

# If run requested, build first (recursively, but only once)
if [ "$RUN_AFTER" = "yes" ]; then
    valhalla_banner
    echo -e "${Y}Building $BUILD_TYPE + launching...${N}"
    "$0" "${@:1:$#-1}"  # re-invoke without 'run'
    echo -e "${M}FIRST LIGHT — ENTERING VALHALLA${N}"
    "./$BIN_DIR/$BINARY" "${@: -1}"  # pass remaining args to binary
    exit $?
fi

# =============================================================================
# SHADER SMART HASH CACHING — FIXED & ETERNAL
# =============================================================================
SHADER_CACHE=".shader_hash_cache"
compile_shader_if_changed() {
    local src="$1"
    local dst="$2"
    local stage="$3"          # ← NOW PASSED CORRECTLY
    local hash_new=$(sha256sum "$src" | cut -d' ' -f1)
    local hash_old=$(grep -F "$src" "$SHADER_CACHE" 2>/dev/null | awk '{print $1}' || echo "")

    if [ "$hash_new" != "$hash_old" ]; then
        echo -e "${C}Shader → $(basename "$src") → $stage.spv${N}"
        glslc -std=460 -fshader-stage="$stage" \
              -I shaders -I include/engine/Vulkan \
              "$src" -o "$dst" || { echo -e "${R}Shader compilation failed${N}"; exit 1; }
        sed -i "/^$hash_new /d" "$SHADER_CACHE" 2>/dev/null || true
        sed -i "/$src/d" "$SHADER_CACHE" 2>/dev/null || true
        echo "$hash_new $src $stage" >> "$SHADER_CACHE"
    fi
}

# Rebuild only changed shaders — stage extracted from filename (correctly!)
if [ -d "shaders" ]; then
    find shaders -type f -name "*.glsl" | while read src; do
        filename=$(basename "$src")
        dir=$(dirname "$src" | sed 's|^shaders/||')

        # Extract stage from filename — bulletproof
        case "$filename" in
            *vert.glsl|*_vert.glsl)   stage="vert" ;;
            *frag.glsl|*_frag.glsl)   stage="frag" ;;
            *comp.glsl|*_comp.glsl)   stage="comp" ;;
            *rgen.glsl|*_rgen.glsl)   stage="rgen" ;;
            *rmiss.glsl|*_rmiss.glsl) stage="rmiss" ;;
            *rchit.glsl|*_rchit.glsl) stage="rchit" ;;
            *rahit.glsl|*_rahit.glsl) stage="rahit" ;;
            *rint.glsl|*_rint.glsl)   stage="rint" ;;
            *rcall.glsl|*_rcall.glsl) stage="rcall" ;;
            *) continue ;;
        esac

        dst="$SHADERS_OUT/$dir/${filename%.glsl}.spv"
        mkdir -p "$(dirname "$dst")"
        compile_shader_if_changed "$src" "$dst" "$stage"
    done
fi

# =============================================================================
# BUILD — MAKE BY DEFAULT, NINJA OPTIONAL
# =============================================================================

valhalla_banner
echo -e "${C}AMOURANTH RTX — LINUX BUILD — STONEKEY v∞ ACTIVE${N}"
echo -e "Mode     : ${Y}$BUILD_TYPE${N} $( [ "$USE_NINJA" = "yes" ] && echo "(Ninja)" || echo "(Make — default)" )"
echo -e "Standard : ${W}C++23 ENFORCED${N}"
echo -e "Shaders  : Smart hash caching active${N}"
echo

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

GENERATOR="Unix Makefiles"
[ "$USE_NINJA" = "yes" ] && GENERATOR="Ninja"

if [ ! -f CMakeCache.txt ] || ! grep -q "$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${Y}Configuring CMake ($BUILD_TYPE) → $GENERATOR${N}"
    cmake .. \
        -G "$GENERATOR" \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
else
    echo -e "${G}CMake cached — incremental build engaged${N}"
fi

echo -e "${M}Building with all cores — PINK PHOTONS RISING...${N}"
cmake --build . --config $BUILD_TYPE -j$(nproc)

mkdir -p "../$BIN_DIR"
cp amouranth_engine "../$BIN_DIR/$BINARY" 2>/dev/null || cp Navigator "../$BIN_DIR/$BINARY" || true

echo
echo -e "${G}BUILD SUCCESS — FIRST LIGHT ACHIEVED${N}"
echo -e "Binary   → ${W}$BIN_DIR/$BINARY${N}"
echo -e "Run with → ${C}./linux.sh run${N}  or directly ${C}./$BIN_DIR/$BINARY${N}"
echo -e "${M}PINK PHOTONS ETERNAL — VALHALLA AWAITS — BLUE CHECKMARK SECURE${N}"

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