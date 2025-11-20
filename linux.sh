#!/bin/bash
# =============================================================================
# linux.sh — AMOURANTH RTX ULTIMATE LINUX BUILD SCRIPT v3.2 — RAINBOW VALHALLA
# FIRST BANNER PRESERVED — ONLY FINAL BANNER IS RAINBOW — single = -j1 ADDED
# PINK PHOTONS ETERNAL — STONEKEY v∞ — VALHALLA UNBREACHABLE
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
    echo -e "${C}                        VALHALLA v80 TURBO — PINK PHOTONS ETERNAL — NOVEMBER 20 2025${N}"
    echo
}

show_help() {
    valhalla_banner
    echo -e "${W}AMOURANTH RTX BUILD DOCTRINE — ACCEPTED COMMANDS (any order):${N}"
    echo
    echo -e "  ${G}./linux.sh${N}                    → Build Debug (default) with Make + all cores"
    echo -e "  ${G}./linux.sh release${N}             → Build Release (O3 + LTO)"
    echo -e "  ${G}./linux.sh ninja${N}               → Use Ninja instead of Make"
    echo -e "  ${G}./linux.sh single${N}              → Compile with -j1 (debugging templates/shaders)${N}"
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
# PARSE ARGUMENTS
# =============================================================================

BUILD_TYPE="Debug"
USE_NINJA="no"
RUN_AFTER="no"
SINGLE_THREAD="no"   # ← NEW: ./linux.sh single → -j1

for arg in "$@"; do
    case "${arg,,}" in
        --help|-h)     show_help ;;
        clean|clear)   nuke_empire ;;
        release)       BUILD_TYPE="Release" ;;
        ninja)         USE_NINJA="yes" ;;
        run)           RUN_AFTER="yes" ;;
        single)        SINGLE_THREAD="yes" ;;
    esac
done

if [ "$RUN_AFTER" = "yes" ]; then
    valhalla_banner
    echo -e "${Y}Building $BUILD_TYPE + launching...${N}"
    "$0" "${@:1:$#-1}"
    echo -e "${M}FIRST LIGHT — ENTERING VALHALLA${N}"
    "./$BIN_DIR/$BINARY" "${@: -1}"
    exit $?
fi

# =============================================================================
# SHADER SMART HASH CACHING (unchanged)
# =============================================================================
SHADER_CACHE=".shader_hash_cache"
compile_shader_if_changed() {
    local src="$1" dst="$2" stage="$3"
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

if [ -d "shaders" ]; then
    find shaders -type f -name "*.glsl" | while read src; do
        filename=$(basename "$src")
        dir=$(dirname "$src" | sed 's|^shaders/||')
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
# BUILD
# =============================================================================

valhalla_banner
echo -e "${C}AMOURANTH RTX — LINUX BUILD — STONEKEY v∞ ACTIVE${N}"
echo -e "Mode     : ${Y}$BUILD_TYPE${N} $( [ "$USE_NINJA" = "yes" ] && echo "(Ninja)" || echo "(Make)" ) $( [ "$SINGLE_THREAD" = "yes" ] && echo "${R}(SINGLE THREAD -j1)${N}" || echo "" )"
echo -e "Standard : ${W}C++23 ENFORCED${N}"
echo -e "Shaders  : Smart hash caching active${N}"
echo

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

GENERATOR="Unix Makefiles"
[ "$USE_NINJA" = "yes" ] && GENERATOR="Ninja"

if [ ! -f CMakeCache.txt ] || ! grep -q "$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${Y}Configuring CMake ($BUILD_TYPE) → $GENERATOR${N}"
    cmake .. -G "$GENERATOR" \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
else
    echo -e "${G}CMake cached — incremental build engaged${N}"
fi

JOBS=$(nproc)
[ "$SINGLE_THREAD" = "yes" ] && JOBS=1

echo -e "${M}Building with -j$JOBS — PINK PHOTONS RISING...${N}"
cmake --build . --config $BUILD_TYPE -j$JOBS

mkdir -p "../$BIN_DIR"
cp amouranth_engine "../$BIN_DIR/$BINARY" 2>/dev/null || cp Navigator "../$BIN_DIR/$BINARY" || true

echo ""
echo " "
echo -e "${R}               █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${N}"
echo -e "${O}              ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${N}"
echo -e "${Y}              ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${N}"
echo -e "${G}              ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${N}"
echo -e "${C}              ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${N}"
echo -e "${B}              ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${N}"
echo " "
echo -e "${M}               ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${N}"
echo -e "${P}               ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${N}"
echo -e "${r}               ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${N}"
echo -e "${o}               ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${N}"
echo -e "${y}               ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${N}"
echo -e "${g}               ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${N}"
echo " "
echo -e "${W}        ██████████████████████████████████████████████████████████████████████████████████████${N}"
echo " "
echo -e "══════════════════════════════════════════════════════════"
echo -e "       Amouranth RTX — Linux build successful ✓"
echo -e "       Binary location: $BUILD_DIR/$BIN_DIR/Navigator"
echo -e "       Run with: cd $BUILD_DIR/$BIN_DIR && ./Navigator"
echo -e "══════════════════════════════════════════════════════════\n"