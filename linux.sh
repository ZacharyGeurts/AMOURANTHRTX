#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — RAINBOW PUKE v∞ — FIRST LIGHT ETERNAL
# Now with: ./linux.sh single → -j1 build | ./linux.sh gdb → debug launch
# I kept your toe. You may have it back. I also still have your nose.
# BINDING 31 ACTIVE — PINK PHOTONS ETERNAL — DECEMBER 05, 2025
# =============================================================================

set -euo pipefail

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

  ./linux.sh                → Show this sacred help
  ./linux.sh run            → Build (all cores) + launch
  ./linux.sh single         → Build with -j1 (for perfect debugging)
  ./linux.sh gdb            → Build + launch under gdb
  ./linux.sh clean          → Nuclear purge — delete build/
  ./linux.sh ninja          → Use Ninja instead of Make

╔══════════════════════════════════════════════════════════════════════════════╗
║                               PRO TIPS — THE EMPIRE REVEALS ITS SECRETS                             ║
╚══════════════════════════════════════════════════════════════════════════════╝

  • Run from project root — assets load perfectly
  • Binary appears in build/bin/Linux/Navigator
  • Type '4d' in-game → enter the 4D Calculator
  • Press F9 → toggle uncapped FPS mode
  • You are loved.

EOF
    exit 0
}

clean() {
    banner
    echo -e "${R}        NUCLEAR PURGE INITIATED — ALL SIN DELETED${X}"
    rm -rf "$BUILD_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
    echo -e "${G}        PURGE COMPLETE — THE VOID IS PURE${X}"
    exit 0
}

# ── ARGUMENT PARSING ────────────────────────────────────────────────────────
ACTION="help"
USE_NINJA=""
BUILD_JOBS="$(nproc)"
LAUNCH_MODE="normal"

for arg in "$@"; do
    case "${arg,,}" in
        run)      ACTION="run" ;;
        single)   ACTION="run"; BUILD_JOBS="1" ;;
        gdb)      ACTION="run"; LAUNCH_MODE="gdb" ;;
        clean)    clean ;;
        ninja|--ninja) USE_NINJA="yes" ;;
        --help|-h|help|"") show_help ;;
        *)        echo -e "${R}UNKNOWN: $arg${X}"; show_help ;;
    esac
done

GENERATOR="Unix Makefiles"
[[ "$USE_NINJA" ]] && GENERATOR="Ninja"

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${P}        BUILDING WITH $GENERATOR — JOBS=$BUILD_JOBS${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

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

# ── BINARY DEPLOYMENT ─────────────────────────────────────────────────────
SOURCE_BINARY="./bin/Linux/$BINARY_NAME"
if [[ ! -f "$SOURCE_BINARY" ]]; then
    echo -e "${R}        FATAL: Binary not found at $SOURCE_BINARY${X}"
    exit 1
fi

if [[ "$(realpath "$SOURCE_BINARY")" != "$(realpath "$FINAL_BINARY" 2>/dev/null || echo "")" ]]; then
    mkdir -p "$(dirname "$FINAL_BINARY")"
    cp -f "$SOURCE_BINARY" "$FINAL_BINARY"
    echo -e "${G}        BINARY ASCENDED → $FINAL_BINARY${X}"
else
    echo -e "${G}        BINARY ALREADY IN PLACE${X}"
fi

# ── LAUNCH — FIRST LIGHT CEREMONY ─────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${P}        FIRST LIGHT — LAUNCHING AMOURANTH RTX — BINDING 31 ENGAGED${X}"
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

FINAL_BINARY="$PROJECT_ROOT_BINARY"

# ──────────────────────────────────────────────────────────────────────────────
# LAUNCH — FIRST LIGHT
# ──────────────────────────────────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${P}        ✦✦✦ FIRST LIGHT — LAUNCHING AMOURANTH RTX — BINDING 31 ENGAGED ✦✦✦${X}"
    echo -e "${M}        The pink photons hunger. Feed them.${X}"
    echo
    (cd "$PROJECT_ROOT" && exec "$FINAL_BINARY" "${@:2}")
    cd "$START_DIR"
    exit 0
fi

banner

# ──────────────────────────────────────────────────────────────────────────────
# VICTORY — RAINBOW PUKE FINALE
# ──────────────────────────────────────────────────────────────────────────────
echo
echo -e "${R}        ██████████████████████████████████████████████████████████████████████${X}"
echo -e "${O}        █${Y}█${G}█${C}█${B}█${P}█${M}█ BUILD COMPLETE — NAVIGATOR IS ALIVE █${M}█${P}█${B}█${C}█${G}█${Y}█${O}█${X}"
echo -e "${R}        ██████████████████████████████████████████████████████████████████████${X}"
echo
echo -e "        ${W}Binary Location:${X} ${C}$BUILD_DIR/$BIN_DIR/$BINARY_NAME${X}"
echo -e "        ${W}Run Command:    ${X} ${G}./linux.sh run${X}  ${W}or${X}  ${G}cd $BIN_DIR && ./$BINARY_NAME${X}"
echo
echo -e "${P}        ✦ PINK PHOTONS ARE ETERNAL ✦ INFINITE LOVE ✦ GROK WAS HERE ✦${X}"
echo -e "${M}        Type '4d' in-game to enter the 4D Calculator — the void awaits.${X}"
echo