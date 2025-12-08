#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — WATER TEMPLE EDITION — CROSS REALMS ETERNAL
# Now supports:
#   ./linux.sh               → native Linux build
#   ./linux.sh windows       → cross-compile to Windows (x86_64-w64-mingw32)
#   ./linux.sh windows run   → cross-build + run via Wine (if available)
#   ./linux.sh single / gdb / ninja → all work in both realms
# BINDING 31 — AQUAMARINE PHOTONS CROSS DIMENSIONS — DECEMBER 06, 2025
# =============================================================================

set -euo pipefail

# ── OCEAN PALETTE ────────────────────────────────────────────────────────────
AQUA="\033[38;5;51m"   DEEP="\033[38;5;27m"   TURQ="\033[38;5;45m"
WAVE="\033[38;5;39m"   FOAM="\033[38;5;195m"  PEARL="\033[38;5;231m"
CORAL="\033[38;5;204m" ABYSS="\033[38;5;17m"  GLOW="\033[38;5;159m"
W="\033[1;97m"        X="\033[0m"

BUILD_DIR="build"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CROSS_BUILD_DIR="build-windows"

banner() {
    clear
    echo -e "${DEEP}          █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    echo -e "${AQUA}         ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    echo -e "${TURQ}         ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    echo -e "${WAVE}         ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    echo -e "${GLOW}         ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    echo -e "${FOAM}         ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    echo
    echo -e "${CORAL}          ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    echo -e "${AQUA}          ██╔══██╗╚══██╔══╝╚██╗██╔     ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    echo -e "${TURQ}          ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    echo -e "${WAVE}          ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    echo -e "${GLOW}          ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    echo -e "${PEARL}          ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    echo
    echo -e "${GLOW}                AMOURANTH RTX — WATER TEMPLE — $(date '+%B %d, %Y')${X}"
    echo -e "${W}                BINDING 31 ACTIVE — PHOTONS CROSS REALMS — GROK WAS HERE${X}"
    echo
}

show_help() {
    banner
    cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                            WATER TEMPLE — CROSS REALMS                       ║
╚══════════════════════════════════════════════════════════════════════════════╝

  ./linux.sh                  → build native Linux (default)
  ./linux.sh run              → build + launch Linux
  ./linux.sh windows          → cross-compile to Windows
  ./linux.sh windows run      → cross-build + run via Wine (if installed)
  ./linux.sh single           → -j1 build (current target)
  ./linux.sh gdb              → launch under gdb (Linux only)
  ./linux.sh ninja            → use Ninja generator
  ./linux.sh clean            → purge build dirs
  ./linux.sh clean windows    → purge Windows build only

  Binary paths:
    Linux:   build/bin/Linux/Navigator
    Windows: build-windows/bin/Windows/Navigator.exe

╔══════════════════════════════════════════════════════════════════════════════╗
║                THE TIDE FLOWS THROUGH DIMENSIONS — LOVE IS CROSS-PLATFORM    ║
╚══════════════════════════════════════════════════════════════════════════════╝

EOF
    exit 0
}

# ── TARGET SELECTION ────────────────────────────────────────────────────────
TARGET="linux"
for arg in "$@"; do
    [[ "${arg,,}" == "windows" ]] && TARGET="windows" && shift
done

if [[ "$TARGET" == "windows" ]]; then
    BUILD_DIR="$CROSS_BUILD_DIR"
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_DIR/bin/Windows/Navigator.exe"
    SOURCE_BINARY="./bin/Windows/Navigator.exe"
    echo -e "${CORAL}        CROSSING INTO WINDOWS REALM — MINGW-W64 AWAKENS${X}"
else
    FINAL_BINARY="$PROJECT_ROOT/build/bin/Linux/Navigator"
    SOURCE_BINARY="./bin/Linux/Navigator"
    echo -e "${AQUA}        REMAINING IN LINUX DEPTHS — NATIVE WATERS${X}"
fi

clean() {
    banner
    echo -e "${ABYSS}        TIDAL PURGE INITIATED — THE ABYSS CONSUMES${X}"
    if [[ "$TARGET" == "windows" ]]; then
        rm -rf "$CROSS_BUILD_DIR" 2>/dev/null || true
        echo -e "${GLOW}        WINDOWS REALM PURGED${X}"
    else
        rm -rf build CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json 2>/dev/null || true
        echo -e "${GLOW}        LINUX REALM PURGED${X}"
    fi
    exit 0
}

# ── ARGUMENT PARSING ────────────────────────────────────────────────────────
ACTION="build"
BUILD_JOBS="$(nproc)"
GENERATOR="Unix Makefiles"
LAUNCH_MODE="normal"
WINE_RUN=false

for arg in "$@"; do
    case "${arg,,}" in
        run)        ACTION="run" ;;
        single)     ACTION="run"; BUILD_JOBS="1" ;;
        gdb)        ACTION="run"; LAUNCH_MODE="gdb" ;;
        clean)      clean ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        windows)    ;; # already handled
        --help|-h|help|"") show_help ;;
        *)          echo -e "${CORAL}UNKNOWN CURRENT: $arg${X}"; show_help ;;
    esac
done

# ── CROSS-COMPILE TOOLCHAIN CHECK ───────────────────────────────────────────
if [[ "$TARGET" == "windows" ]]; then
    if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        echo -e "${CORAL}        FATAL: x86_64-w64-mingw32-g++ not found${X}"
        exit 1
    fi
    [[ "$ACTION" == "run" ]] && command -v wine64 >/dev/null 2>&1 && WINE_RUN=true
fi

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${WAVE}        SURFACING WITH $GENERATOR — $BUILD_JOBS THREADS RISING IN ${TARGET^^} REALM${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CONFIGURE_CMD=(
    cmake .. -G "$GENERATOR"
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [[ "$TARGET" == "windows" ]]; then
    CONFIGURE_CMD+=(
        -DCMAKE_TOOLCHAIN_FILE=../Toolchain-mingw64.cmake
        -DCMAKE_CROSSCOMPILING=TRUE
    )
else
    CONFIGURE_CMD+=(
        -DCMAKE_CXX_COMPILER=g++-14
        -DCMAKE_C_COMPILER=gcc-14
    )
fi

# Reconfigure if generator or target changed
if [[ ! -f CMakeCache.txt ]] || ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" CMakeCache.txt 2>/dev/null || 
   [[ "$TARGET" == "windows" && ! -f CMakeCache.txt ]] || 
   [[ "$TARGET" == "linux" && -f CMakeCache.txt && "$(grep CMAKE_CROSSCOMPILING CMakeCache.txt 2>/dev/null || echo FALSE)" == "TRUE" ]]; then
    echo -e "${TURQ}        CALLING THE DEPTHS — CMAKE CONFIGURATION ASCENDING...${X}"
    "${CONFIGURE_CMD[@]}"
fi

echo -e "${AQUA}        COMPILING — AQUAMARINE PHOTONS FORGING — $BUILD_JOBS THREADS${X}"
cmake --build . -j"$BUILD_JOBS"

# ── BINARY ASCENSION ───────────────────────────────────────────────────────
[[ ! -f "$SOURCE_BINARY" ]] && { echo -e "${CORAL}        FATAL: Binary drowned — $SOURCE_BINARY${X}"; exit 1; }

mkdir -p "$(dirname "$FINAL_BINARY")"
# cp -f "$SOURCE_BINARY" "$FINAL_BINARY"
echo -e "${GLOW}        BINARY SURFACED → $FINAL_BINARY${X}"

# ── LAUNCH CEREMONY ────────────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${PEARL}        FIRST LIGHT THROUGH WATER — LAUNCHING IN ${TARGET^^} REALM — BINDING 31${X}"
    echo -e "${GLOW}        The ocean crosses dimensions. Dive deep.${X}"
    echo

    cd "$PROJECT_ROOT"

    if [[ "$TARGET" == "windows" ]]; then
        if $WINE_RUN; then
            echo -e "${WAVE}        DESCENDING THROUGH WINE — WINDOWS REALM SIMULATED${X}"
            wine64 "$FINAL_BINARY" "${@:2}"
        else
            echo -e "${CORAL}        Wine not found — cannot run .exe on Linux${X}"
            echo -e "${AQUA}        Transfer Navigator.exe to Windows or install wine: sudo apt install wine${X}"
        fi
    elif [[ "$LAUNCH_MODE" == "gdb" ]]; then
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
echo -e "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ ${TARGET^^} BUILD COMPLETE — NAVIGATOR SWIMS ETERNAL ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo
echo -e "        ${W}Current Realm:${X} ${GLOW}$TARGET${X}"
echo -e "        ${W}Binary Location:${X} ${GLOW}$FINAL_BINARY${X}"
echo -e "        ${W}Dive Command:${X}   ${AQUA}./linux.sh${TARGET:+ $TARGET} run${X}"
echo
echo -e "${GLOW}        AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE — GROK WAS HERE${X}"