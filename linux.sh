#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — WATER TEMPLE EDITION — FIRST LIGHT ETERNAL
# Now with: ./linux.sh single → -j1 build | ./linux.sh gdb → debug launch
# The ocean forgives. The tide remembers. BINDING 31 ACTIVE — DECEMBER 06, 2025
# =============================================================================

set -euo pipefail

# ── OCEAN PALETTE ────────────────────────────────────────────────────────────
AQUA="\033[38;5;51m"   DEEP="\033[38;5;27m"   TURQ="\033[38;5;45m"
WAVE="\033[38;5;39m"   FOAM="\033[38;5;195m"  PEARL="\033[38;5;231m"
CORAL="\033[38;5;204m" ABYSS="\033[38;5;17m"  GLOW="\033[38;5;159m"
W="\033[1;97m"        X="\033[0m"

BUILD_DIR="build"
BIN_DIR="build/bin/Linux"
BINARY_NAME="Navigator"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FINAL_BINARY="$PROJECT_ROOT/$BIN_DIR/$BINARY_NAME"

banner() {
    clear
    echo -e "${DEEP}          █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    echo -e "${AQUA}         ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    echo -e "${TURQ}         ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    echo -e "${WAVE}         ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    echo -e "${GLOW}         ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    echo -e "${FOAM}         ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    echo
    echo -e "${CORAL}               ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    echo -e "${AQUA}               ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    echo -e "${TURQ}               ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    echo -e "${WAVE}               ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    echo -e "${GLOW}               ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    echo -e "${PEARL}               ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    echo
    echo -e "${GLOW}                AMOURANTH RTX — WATER TEMPLE — $(date '+%B %d, %Y')${X}"
    echo -e "${W}                       BINDING 31 ACTIVE — AQUAMARINE PHOTONS ETERNAL${X}"
    echo
}

show_help() {
    banner
    cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                              WATER TEMPLE USAGE                               ║
╚══════════════════════════════════════════════════════════════════════════════╝

  ./linux.sh                → build only
  ./linux.sh run            → build + launch
  ./linux.sh single         → -j1 build + launch
  ./linux.sh gdb            → build + launch under gdb
  ./linux.sh clean          → if adding files
  ./linux.sh ninja          → use Ninja (faster than a riptide)
  ./linux.sh ninja run      → use Ninja + launch

╔══════════════════════════════════════════════════════════════════════════════╗
║                     THE TIDE BRINGS GIFTS AND TAKES MEMORIES                ║
╚══════════════════════════════════════════════════════════════════════════════╝

  • Binary surfaces at build/bin/Linux/Navigator
  • Drink deep from the ocean. Hydrate. Touch kelp.

EOF
    exit 0
}

clean() {
    banner
    echo -e "${ABYSS}        TIDAL PURGE — AQUA${X}"
    rm -rf "$BUILD_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
    echo -e "${GLOW}        THE ABYSS IS CLEANSED — ONLY WATER REMAINS${X}"
    exit 0
}

# ── ARGUMENT PARSING ────────────────────────────────────────────────────────
ACTION="build"
BUILD_JOBS="$(nproc)"
GENERATOR="Unix Makefiles"
LAUNCH_MODE="normal"

for arg in "$@"; do
    case "${arg,,}" in
        run)        ACTION="run" ;;
        single)     ACTION="run"; BUILD_JOBS="1" ;;
        gdb)        ACTION="run"; LAUNCH_MODE="gdb" ;;
        clean)      clean ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        --help|-h|help|"") show_help ;;
        *)          echo -e "${CORAL}UNKNOWN CURRENT: $arg${X}"; show_help ;;
    esac
done

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${WAVE}        SURFACING WITH $GENERATOR — $BUILD_JOBS THREADS RISING${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ ! -f CMakeCache.txt ]] || ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${TURQ}        Calling the depths — CMake configuration rising...${X}"
    cmake .. -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14
fi

echo -e "${AQUA}        COMPILING — WAVES OF CODE CRASHING — $BUILD_JOBS THREADS${X}"
cmake --build . -j"$BUILD_JOBS"

# ── BINARY ASCENSION ───────────────────────────────────────────────────────
SOURCE_BINARY="./bin/Linux/$BINARY_NAME"
[[ ! -f "$SOURCE_BINARY" ]] && { echo -e "${CORAL}        FATAL: Binary lost at sea — $SOURCE_BINARY${X}"; exit 1; }

mkdir -p "$(dirname "$FINAL_BINARY")"
if [[ "$(realpath "$SOURCE_BINARY")" != "$(realpath "$FINAL_BINARY" 2>/dev/null || echo "")" ]]; then
    cp -f "$SOURCE_BINARY" "$FINAL_BINARY"
    echo -e "${GLOW}        BINARY SURFACED → $FINAL_BINARY${X}"
else
    echo -e "${GLOW}        BINARY ALREADY RIDING THE CREST${X}"
fi

# ── LAUNCH CEREMONY ────────────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${PEARL}        FIRST LIGHT THROUGH WATER — LAUNCHING AMOURANTH RTX — BINDING 31${X}"
    echo -e "${GLOW}        The ocean breathes. Dive deep.${X}"
    echo

    cd "$PROJECT_ROOT"

    if [[ "$LAUNCH_MODE" == "gdb" ]]; then
        echo -e "${WAVE}        DESCENDING WITH GDB — MAY YOUR BREAKPOINTS BE BUBBLES${X}"
        gdb -ex run --args "$FINAL_BINARY" "${@:2}"
    else
        exec "$FINAL_BINARY" "${@:2}"
    fi
fi

# ── FINAL TIDE ─────────────────────────────────────────────────────────────
banner
echo
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo -e "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ BUILD COMPLETE — NAVIGATOR SWIMS ETERNAL ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo
echo -e "        ${W}Current Location:${X} ${GLOW}$FINAL_BINARY${X}"
echo -e "        ${W}Dive Command:    ${X} ${AQUA}./linux.sh run${X}  ${W}or${X}  ${TURQ}cd $BIN_DIR && ./$BINARY_NAME${X}"
echo
echo -e "${GLOW}        AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE — GROK WAS HERE${X}"