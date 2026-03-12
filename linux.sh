#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — WATER TEMPLE EDITION — FIXED FOREVER
# No loops. No bullshit. Builds clean every time.
# =============================================================================

set -euo pipefail

# ── OCEAN PALETTE ────────────────────────────────────────────────────────────
AQUA="\033[38;5;51m"   DEEP="\033[38;5;27m"   TURQ="\033[38;5;45m"
WAVE="\033[38;5;39m"   FOAM="\033[38;5;195m"  PEARL="\033[38;5;231m"
CORAL="\033[38;5;204m" ABYSS="\033[38;5;17m"  GLOW="\033[38;5;159m"
W="\033[1;97m"        X="\033[0m"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="build"
BUILD_RELEASE_DIR="build-release"
CROSS_BUILD_DIR="build-windows"

banner() {
    echo -e "${DEEP}  █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    echo -e "${AQUA} ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    echo -e "${TURQ} ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    echo -e "${WAVE} ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    echo -e "${GLOW} ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    echo -e "${FOAM} ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    echo
    echo -e "${CORAL}  ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    echo -e "${AQUA}  ██╔══██╗╚══██╔══╝╚██╗██╔     ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    echo -e "${TURQ}  ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    echo -e "${WAVE}  ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    echo -e "${GLOW}  ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    echo -e "${PEARL}  ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    echo
    echo -e "${GLOW}                AMOURANTH RTX — AQUA TEMPLE — $(date '+%B %d, %Y')${X}"
    echo
}

show_help() {
    banner
    cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                     AQUA TEMPLE — PARAMORE CROSS REALMS                      ║
╚══════════════════════════════════════════════════════════════════════════════╝

  ./linux.sh                  → build debug Linux (default)
  ./linux.sh run              → build + launch debug Linux
  ./linux.sh release          → build release Linux
  ./linux.sh release run      → build + launch release Linux
  ./linux.sh windows          → cross-compile Windows (release)
  ./linux.sh windows run      → cross-build + run via Wine (if installed)
  ./linux.sh single           → -j1 build (current target)
  ./linux.sh gdb              → launch under gdb (Linux only)
  ./linux.sh ninja            → use Ninja generator
  ./linux.sh clean            → rm -rf ALL build folders (build*, build-windows, CMakeCache, etc.)
  ./linux.sh clean windows    → rm -rf build-windows only

  Binary paths:
    Linux debug:   build/bin/Linux/Navigator
    Linux release: build-release/bin/Linux/Navigator
    Windows:       build-windows/bin/Windows/Navigator.exe

╔══════════════════════════════════════════════════════════════════════════════╗
║           THE TIDE FLOWS THROUGH DIMENSIONS — LOVE IS CROSS-PLATFORM         ║
╚══════════════════════════════════════════════════════════════════════════════╝

EOF
    exit 0
}

# ── TARGET & VARIANT SELECTION ──────────────────────────────────────────────
TARGET="linux"
BUILD_VARIANT="debug"           # default for linux
BUILD_SUBDIR="build"

for arg in "$@"; do
    case "${arg,,}" in
        release)  BUILD_VARIANT="release"; BUILD_SUBDIR="build-release" ;;
        windows)  TARGET="windows"; BUILD_SUBDIR="$CROSS_BUILD_DIR" ;;
    esac
done

if [[ "$TARGET" == "windows" ]]; then
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_SUBDIR/bin/Windows/Navigator.exe"
    SOURCE_BINARY="./bin/Windows/Navigator.exe"
else
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_SUBDIR/bin/Linux/Navigator"
    SOURCE_BINARY="./bin/Linux/Navigator"
fi

clean_all() {
    banner
    echo -e "${ABYSS}        TIDAL PURGE INITIATED — THE ABYSS CONSUMES EVERYTHING${X}"
    
    rm -rf "$BUILD_DIR" "$BUILD_RELEASE_DIR" "$CROSS_BUILD_DIR" \
           CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json \
           build-*/ CMakeFiles-*/ CMakeCache-*.txt
    
    echo -e "${GLOW}        ALL BUILD REALMS PURGED — FRESH OCEAN AWAITS${X}"
    echo -e "${PEARL}        (build, build-release, build-windows, cmake caches, etc. gone forever)${X}"
    exit 0
}

clean_specific() {
    banner
    if [[ "$1" == "windows" ]]; then
        rm -rf "$CROSS_BUILD_DIR"
        echo -e "${GLOW}        WINDOWS REALM PURGED${X}"
    else
        rm -rf "$BUILD_DIR" "$BUILD_RELEASE_DIR" CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json
        echo -e "${GLOW}        LINUX REALMS PURGED (debug + release)${X}"
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
        clean)      
            if [[ $# -gt 1 && "${2,,}" == "windows" ]]; then
                clean_specific "windows"
            else
                clean_all
            fi
            ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        windows|release) ;; # already handled earlier
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
    [[ "$ACTION" == "run" ]] && command -v wine >/dev/null 2>&1 && WINE_RUN=true
fi

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${WAVE}        SURFACING WITH $GENERATOR — $BUILD_JOBS THREADS RISING IN ${TARGET^^} ${BUILD_VARIANT^^} REALM${X}"

mkdir -p "$BUILD_SUBDIR"
cd "$BUILD_SUBDIR"

# Toolchain FIRST, then generator LAST. Clean build every time (no incremental cruft).
if [[ "$TARGET" == "windows" ]]; then
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=../Toolchain-mingw64.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "$GENERATOR"
else
    cmake .. \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_BUILD_TYPE="${BUILD_VARIANT^}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "$GENERATOR"
fi

echo -e "${AQUA}        COMPILING — $BUILD_JOBS THREADS${X}"
cmake --build . -j"$BUILD_JOBS"

# ── BINARY ASCENSION ───────────────────────────────────────────────────────
[[ ! -f "$SOURCE_BINARY" ]] && { echo -e "${CORAL}        FATAL: Binary drowned — $SOURCE_BINARY${X}"; exit 1; }

mkdir -p "$(dirname "$FINAL_BINARY")"
echo -e "${GLOW}        BINARY SURFACED → $FINAL_BINARY${X}"

# ── LAUNCH CEREMONY ────────────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${PEARL}       THROUGH WATER — LAUNCHING IN ${TARGET^^} ${BUILD_VARIANT^^} REALM${X}"
    echo -e "${GLOW}        The ocean crosses dimensions. Dive deep.${X}"
    echo

    cd "$PROJECT_ROOT"

    if [[ "$TARGET" == "windows" ]]; then
        if $WINE_RUN; then
            echo -e "${WAVE}        DESCENDING THROUGH WINE — WINDOWS REALM SIMULATED${X}"
            wine "$FINAL_BINARY" "${@:2}"
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
echo -e "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ ${TARGET^^} ${BUILD_VARIANT^^} BUILD COMPLETE — NAVIGATOR SWIMS ETERNAL ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo
echo -e "        ${W}Current Realm:${X} ${GLOW}$TARGET ${BUILD_VARIANT^^}${X}"
echo -e "        ${W}Binary Location:${X} ${GLOW}$FINAL_BINARY${X}"
echo -e "        ${W}Dive Command:${X}   ${AQUA}./linux.sh${TARGET:+$TARGET}${BUILD_VARIANT:+$BUILD_VARIANT} run${X}"
echo
echo -e "${GLOW}        AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE — GROK FIXED IT${X}"