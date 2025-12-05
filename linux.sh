#!/usr/bin/env bash
# =============================================================================
# linux.sh — Amouranth RTX — THE ONE THAT JUST WORKS™ (Lazy Genius Edition)
# Always runs from project root → assets load
# Always returns you to where you started → no mess
# =============================================================================

set -euo pipefail

BUILD_DIR="build"
BIN_DIR="build/bin/Linux"
BINARY_NAME="Navigator"

# Colors
R="\033[0;31m" G="\033[0;32m" Y="\033[1;33m" B="\033[0;34m" M="\033[0;35m" C="\033[0;36m" W="\033[1;37m" N="\033[0m"

# Save where we started — we will return here like a good warrior
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR" && pwd)"   # in case script is symlinked

banner() {
    clear
    cat << "EOF"
          █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗
         ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║
         ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║
         ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║
         ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║
         ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝
EOF
    echo -e "${C}                       AMOURANTH RTX — $(date '+%B %d, %Y')${N}\n"
}

show_help() {
    banner
    echo -e "${W}Usage:${N}"
    echo -e "  ./linux.sh            → help"
    echo -e "  ./linux.sh run        → build + run (assets always load)"
    echo -e "  ./linux.sh clean      → nuke everything"
    echo -e "  ./linux.sh --ninja run → use Ninja"
    echo
    exit 0
}

clean() {
    banner
    echo -e "${Y}Nuking build directory...${N}"
    rm -rf "$BUILD_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
    echo -e "${G}Clean complete.${N}"
    exit 0
}

# ─── Args ───
ACTION="help"
USE_NINJA=""

for arg in "$@"; do
    case "${arg,,}" in
        run)           ACTION="run" ;;
        clean)         clean ;;
        --ninja|ninja) USE_NINJA="yes" ;;
        --help|-h|help|"") show_help ;;
        *)             echo -e "${R}Invalid: $arg${N}"; show_help ;;
    esac
done

[[ "$USE_NINJA" && "$ACTION" == "help" ]] && ACTION="run"
GENERATOR="Unix Makefiles"
[[ "$USE_NINJA" ]] && GENERATOR="Ninja"

# ─── Build ───
banner
echo -e "${C}Building with $GENERATOR (-j$(nproc))...${N}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ ! -f CMakeCache.txt ]] || ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${Y}Configuring CMake...${N}"
    cmake .. -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14
fi

echo -e "${Y}Compiling...${N}"
cmake --build . -j$(nproc)

# ─── Binary handling ───
SOURCE_BINARY="./bin/Linux/$BINARY_NAME"
DEST_BINARY="../$BIN_DIR/$BINARY_NAME"

if [[ ! -f "$SOURCE_BINARY" ]]; then
    echo -e "${R}FATAL: Binary not found at $SOURCE_BINARY${N}"
    exit 1
fi

mkdir -p "$(dirname "$DEST_BINARY")"
if [[ "$(realpath "$SOURCE_BINARY")" != "$(realpath "$DEST_BINARY" 2>/dev/null || echo "")" ]]; then
    cp -f "$SOURCE_BINARY" "$DEST_BINARY"
    echo -e "${G}Binary deployed → $DEST_BINARY${N}"
else
    echo -e "${G}Binary already in position${N}"
fi

FINAL_BINARY="$(realpath "$DEST_BINARY")"

# ─── Run — The Lazy Genius Way (assets load + we return home) ───
if [[ "$ACTION" == "run" ]]; then
    echo -e "${M}FIRST LIGHT — Launching from the sacred root...${N}\n"
    # This is the magic: run from project root, then return to where we were
    (cd "$PROJECT_ROOT" && exec "$FINAL_BINARY" "${@:2}")
    # If exec fails (extremely rare), we still return:
    cd "$SCRIPT_DIR"
    exit 0
fi

# Victory
echo
echo -e "${G}Build Complete — Navigator ready${N}"
echo -e "Run: ${M}./linux.sh run${N} or ${M}./$BIN_DIR/$BINARY${N}"
echo

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