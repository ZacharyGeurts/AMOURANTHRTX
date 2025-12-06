#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — RAINBOW PUKE v3 — SAME-FILE ERROR ELIMINATED
# No more cp warnings. No more shame. Only beauty. Only her.
# FIRST LIGHT ACHIEVED — DECEMBER 05, 2025 — BINDING 31 ACTIVE
# =============================================================================

set -euo pipefail

# ──────────────────────────────────────────────────────────────────────────────
# HYPER-VIVID RAINBOW COLOR PALETTE
# ──────────────────────────────────────────────────────────────────────────────
R="\033[38;5;196m" O="\033[38;5;208m" Y="\033[38;5;226m" G="\033[38;5;82m"
C="\033[38;5;51m"  B="\033[38;5;33m"  P="\033[38;5;201m" M="\033[38;5;165m"
W="\033[1;97m"     K="\033[38;5;232m" X="\033[0m"

BUILD_DIR="build"
BIN_DIR="build/bin/Linux"
BINARY_NAME="Navigator"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR" && pwd)"
START_DIR="$(pwd)"

# ──────────────────────────────────────────────────────────────────────────────
# SACRED BANNER — UNTOUCHED PERFECTION
# ──────────────────────────────────────────────────────────────────────────────
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
    echo -e "${P}                  ✦ ✦ ✦  AMOURANTH RTX — FIRST LIGHT — $(date '+%B %d, %Y')  ✦ ✦ ✦${X}"
    echo -e "${W}                        BINDING 31 ACTIVE — PINK PHOTONS ETERNAL${X}"
    echo
}

show_help() { banner; cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                               USAGE GUIDE                                    ║
╚══════════════════════════════════════════════════════════════════════════════╝

  ./linux.sh               → Show this beautiful help
  ./linux.sh run           → Build + launch (assets always load)
  ./linux.sh clean         → Cleanse — delete build/
  ./linux.sh ninja         → Use Ninja (vs cmake)

╔══════════════════════════════════════════════════════════════════════════════╗
║                               PRO TIPS                                       ║
╚══════════════════════════════════════════════════════════════════════════════╝

  • Always run from project root — assets load perfectly
  • Script returns you to build dir on exit
  • Binary ends up in build/bin/Linux/Navigator

EOF
    exit 0
}

clean() {
    banner
    echo -e "${R}        ☢☢☢  INITIATING FULL SYSTEM PURGE — NO SURVIVORS  ☢☢☢${X}"
    echo -e "${Y}        Deleting build directory, caches, sins of the past...${X}"
    rm -rf "$BUILD_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
    echo -e "${G}        PURGE COMPLETE — THE VOID IS CLEAN${X}"
    echo -e "${P}        Ready for rebirth. Type ./linux.sh run to ascend.${X}"
    exit 0
}

ACTION="help"
USE_NINJA=""

for arg in "$@"; do
    case "${arg,,}" in
        run)           ACTION="run" ;;
        clean)         clean ;;
        --ninja|ninja) USE_NINJA="yes" ;;
        --help|-h|help|"") show_help ;;
        *)             echo -e "${R}UNKNOWN COMMAND: $arg — TYPE ./linux.sh FOR HELP${X}"; show_help ;;
    esac
done

[[ "$USE_NINJA" && "$ACTION" == "help" ]] && ACTION="run"
GENERATOR="Unix Makefiles"
[[ "$USE_NINJA" ]] && GENERATOR="Ninja"

banner
echo -e "${P}        ✦ BUILDING WITH $GENERATOR — USING ALL $(nproc) CORES ✦${X}"
echo -e "${C}        Compiling shaders of the gods... patience, mortal...${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ ! -f CMakeCache.txt ]] || ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" CMakeCache.txt 2>/dev/null; then
    echo -e "${Y}        ✦ Configuring CMake — invoking the ancient rites...${X}"
    cmake .. -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14
fi

echo -e "${O}        ✦ COMPILING — PINK PHOTONS MANIFESTING — DO NOT INTERRUPT${X}"
cmake --build . -j$(nproc)

# ──────────────────────────────────────────────────────────────────────────────
# FIXED: ONLY COPY IF PATHS ARE DIFFERENT — NO MORE "SAME FILE" WARNING
# ──────────────────────────────────────────────────────────────────────────────
SOURCE_BINARY="./bin/Linux/$BINARY_NAME"
PROJECT_ROOT_BINARY="$PROJECT_ROOT/$BIN_DIR/$BINARY_NAME"

if [[ ! -f "$SOURCE_BINARY" ]]; then
    echo -e "${R}        ✦ FATAL: Binary not found at $SOURCE_BINARY${X}"
    echo -e "${R}        The gods have rejected us. Check CMake output above.${X}"
    exit 1
fi

# Only copy if source and destination are actually different paths
if [[ "$(realpath "$SOURCE_BINARY")" != "$(realpath "$PROJECT_ROOT_BINARY" 2>/dev/null || echo "")" ]]; then
    mkdir -p "$(dirname "$PROJECT_ROOT_BINARY")"
    cp -f "$SOURCE_BINARY" "$PROJECT_ROOT_BINARY"
    echo -e "${G}        ✓ BINARY DEPLOYED → $PROJECT_ROOT_BINARY${X}"
else
    echo -e "${G}        ✓ BINARY ALREADY IN PLACE → $SOURCE_BINARY${X}"
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