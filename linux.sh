#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — RAINBOW PUKE v∞ — FIRST LIGHT ETERNAL
# ./linux.sh         → build only
# ./linux.sh run     → build + launch
# ./linux.sh single  → -j1 build + launch
# ./linux.sh gdb     → build + launch under gdb
# ./linux.sh ninja   → use Ninja generator
# ./linux.sh clean   → nuke everything
# BINDING 31 ACTIVE — PINK PHOTONS ETERNAL — DECEMBER 06, 2025
# =============================================================================

set -euo pipefail

# ── RAINBOW COLORS ───────────────────────────────────────────────────────────
R="\033[38;5;196m" O="\033[38;5;208m" Y="\033[38;5;226m" G="\033[38;5;82m"
C="\033[38;5;51m"  B="\033[38;5;33m"  P="\033[38;5;201m" M="\033[38;5;165m"
W="\033[1;97m"     X="\033[0m"

BUILD_DIR="build"
BIN_DIR="build/bin/Linux"
BINARY_NAME="Navigator"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FINAL_BINARY="$PROJECT_ROOT/$BIN_DIR/$BINARY_NAME"

banner() {
    clear
    echo -e "${R}          █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    echo -e "${O}         ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    echo -e "${Y}         ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    echo -e "${G}         ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    echo -e "${C}         ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    echo -e "${B}         ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    echo -e "${P}               ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    echo -e "${M}               ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    echo -e "${R}               ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    echo -e "${O}               ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    echo -e "${Y}               ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    echo -e "${G}               ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    echo
    echo -e "${P}                  AMOURANTH RTX — FIRST LIGHT — $(date '+%B %d, %Y')${X}"
    echo -e "${W}                        BINDING 31 ACTIVE — PINK PHOTONS ETERNAL${X}"
    echo
}

show_help() {
    banner
    cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                               USAGE GUIDE                                    ║
╚══════════════════════════════════════════════════════════════════════════════╝

  ./linux.sh                → build only
  ./linux.sh run            → build + launch
  ./linux.sh single         → build with -j1 + launch
  ./linux.sh gdb            → build + launch under gdb
  ./linux.sh clean          → delete build folder & cmake cache
  ./linux.sh ninja          → use Ninja instead of Make
  ./linux.sh ninja run      → Ninja + launch (ultimate speed)

╔══════════════════════════════════════════════════════════════════════════════╗
║                     PINK PHOTONS DEMAND SACRIFICE                           ║
╚══════════════════════════════════════════════════════════════════════════════╝

  • Binary ends up at: build/bin/Linux/Navigator
  • Take 2000mg liposomal vitamin C and touch grass after victory

EOF
    exit 0
}

clean() {
    banner
    echo -e "${R}        NUCLEAR PURGE INITIATED — ALL SIN DELETED${X}"
    rm -rf "$BUILD_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
    echo -e "${G}        PURGE COMPLETE — THE VOID IS PURE AGAIN${X}"
    exit 0
}

# ── ARGUMENT PARSING ────────────────────────────────────────────────────────
ACTION="build"      # build | run
BUILD_JOBS="$(nproc)"
GENERATOR="Unix Makefiles"
LAUNCH_MODE="normal"  # normal | gdb

for arg in "$@"; do
    case "${arg,,}" in
        run)        ACTION="run" ;;
        single)     ACTION="run"; BUILD_JOBS="1" ;;
        gdb)        ACTION="run"; LAUNCH_MODE="gdb" ;;
        clean)      clean ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        --help|-h|help|"") show_help ;;
        *)          echo -e "${R}UNKNOWN ARG: $arg${X}"; show_help ;;
    esac
done

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${P}        BUILDING WITH $GENERATOR — JOBS=$BUILD_JOBS${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Reconfigure if generator changed
if [[ ! -f CMakeCache.txt ]] || ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${Y}        Configuring CMake — awakening the ancient rites...${X}"
    cmake .. -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14
fi

echo -e "${O}        COMPILING — PINK PHOTONS RISING — $BUILD_JOBS THREADS${X}"
cmake --build . -j"$BUILD_JOBS"

# ── COPY BINARY TO PROJECT ROOT ─────────────────────────────────────────────
SOURCE_BINARY="./bin/Linux/$BINARY_NAME"
if [[ ! -f "$SOURCE_BINARY" ]]; then
    echo -e "${R}        FATAL: Binary not found at $SOURCE_BINARY${X}"
    exit 1
fi

mkdir -p "$(dirname "$FINAL_BINARY")"
if [[ "$(realpath "$SOURCE_BINARY")" != "$(realpath "$FINAL_BINARY" 2>/dev/null || echo "")" ]]; then
    cp -f "$SOURCE_BINARY" "$FINAL_BINARY"
    echo -e "${G}        BINARY ASCENDED → $FINAL_BINARY${X}"
else
    echo -e "${G}        BINARY ALREADY PERFECT${X}"
fi

# ── LAUNCH (if requested) ──────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${P}        ✦✦✦ FIRST LIGHT — LAUNCHING AMOURANTH RTX — BINDING 31 ENGAGED ✦✦✦${X}"
    echo -e "${M}        The pink photons hunger. Feed them.${X}"
    echo

    cd "$PROJECT_ROOT"

    if [[ "$LAUNCH_MODE" == "gdb" ]]; then
        echo -e "${Y}        LAUNCHING UNDER GDB — MAY THE BREAKPOINTS BE EVER IN YOUR FAVOR${X}"
        gdb -ex run --args "$FINAL_BINARY" "${@:2}"
    else
        exec "$FINAL_BINARY" "${@:2}"
    fi
fi

# ── VICTORY SCREEN ──────────────────────────────────────────────────────────
banner
echo
echo -e "${R}        ██████████████████████████████████████████████████████████████████████${X}"
echo -e "${O}        █${Y}█${G}█${C}█${B}█${P}█${M}█ BUILD COMPLETE — NAVIGATOR IS ALIVE █${M}█${P}█${B}█${C}█${G}█${Y}█${O}█${X}"
echo -e "${R}        ██████████████████████████████████████████████████████████████████████${X}"
echo
echo -e "        ${W}Binary Location:${X} ${C}$FINAL_BINARY${X}"
echo -e "        ${W}Run Command:    ${X} ${G}./linux.sh run${X}  ${W}or${X}  ${G}cd $BIN_DIR && ./$BINARY_NAME${X}"
echo
echo -e "${P}        ✦ PINK PHOTONS ARE ETERNAL ✦ INFINITE LOVE ✦ GROK FIXED THIS ✦${X}"