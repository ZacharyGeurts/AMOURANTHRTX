#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTHRTX — WATER TEMPLE EDITION
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
WEB_BUILD_DIR="build-web"

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
    echo -e "${GLOW}                AMOURANTHRTX — AQUA TEMPLE — $(date '+%B %d, %Y')${X}"
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
  ./linux.sh windows          → cross-compile Windows (release)
  ./linux.sh web              → build WebAssembly (Emscripten + WebGPU)
  ./linux.sh web run          → build web + launch local server (open in browser)
  ./linux.sh single           → -j1 build (current target)
  ./linux.sh gdb              → launch under gdb (Linux only)
  ./linux.sh ninja            → use Ninja generator
  ./linux.sh clean            → rm -rf ALL build folders

  Binary paths:
    Linux debug:   build/bin/Linux/AMOURANTHRTX
    Linux release: build-release/bin/Linux/AMOURANTHRTX
    Windows:       build-windows/bin/Windows/AMOURANTHRTX.exe
    Web:           build-web/bin/Web/AMOURANTHRTX.html

╔══════════════════════════════════════════════════════════════════════════════╗
║           THE TIDE FLOWS THROUGH DIMENSIONS — LOVE IS CROSS-PLATFORM         ║
╚══════════════════════════════════════════════════════════════════════════════╝

EOF
    exit 0
}

# ── TARGET & VARIANT SELECTION ──────────────────────────────────────────────
TARGET="linux"
BUILD_VARIANT="debug"
BUILD_SUBDIR="build"

for arg in "$@"; do
    case "${arg,,}" in
        release)  BUILD_VARIANT="release"; BUILD_SUBDIR="build-release" ;;
        windows)  TARGET="windows"; BUILD_SUBDIR="build-windows" ;;
        web)      TARGET="web"; BUILD_SUBDIR="build-web" ;;
    esac
done

if [[ "$TARGET" == "windows" ]]; then
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_SUBDIR/bin/Windows/AMOURANTHRTX.exe"
    SOURCE_BINARY="./bin/Windows/AMOURANTHRTX.exe"
elif [[ "$TARGET" == "web" ]]; then
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_SUBDIR/bin/Web/AMOURANTHRTX.html"
    SOURCE_BINARY="./bin/Web/AMOURANTHRTX.html"
else
    FINAL_BINARY="$PROJECT_ROOT/$BUILD_SUBDIR/bin/Linux/AMOURANTHRTX"
    SOURCE_BINARY="./bin/Linux/AMOURANTHRTX"
fi

clean_all() {
    banner
    echo -e "${ABYSS}        TIDAL PURGE INITIATED — THE ABYSS CONSUMES EVERYTHING${X}"
    
    rm -rf "$BUILD_DIR" "$BUILD_RELEASE_DIR" "$CROSS_BUILD_DIR" "$WEB_BUILD_DIR" \
           CMakeCache.txt CMakeFiles .shader_hash_cache compile_commands.json \
           build-*/ CMakeFiles-*/ CMakeCache-*.txt
    
    echo -e "${GLOW}        ALL BUILD REALMS PURGED — FRESH OCEAN AWAITS${X}"
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
        clean)      clean_all ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        windows|release|web) ;; # already handled
        --help|-h|help|"") show_help ;;
        *)          echo -e "${CORAL}UNKNOWN CURRENT: $arg${X}"; show_help ;;
    esac
done

# ── CROSS-COMPILE / EMSCRIPTEN TOOLCHAIN CHECK ──────────────────────────────
if [[ "$TARGET" == "windows" ]]; then
    if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        echo -e "${CORAL}        FATAL: x86_64-w64-mingw32-g++ not found${X}"
        exit 1
    fi
    [[ "$ACTION" == "run" ]] && command -v wine >/dev/null 2>&1 && WINE_RUN=true
elif [[ "$TARGET" == "web" ]]; then
    # No fatal error — we'll handle Emscripten in CMake
    GENERATOR="Unix Makefiles"
fi

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${WAVE}        SURFACING WITH $GENERATOR — $BUILD_JOBS THREADS RISING IN ${TARGET^^} ${BUILD_VARIANT^^} REALM${X}"

mkdir -p "$BUILD_SUBDIR"
cd "$BUILD_SUBDIR"

if [[ "$TARGET" == "windows" ]]; then
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=../Toolchain-mingw64.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "$GENERATOR"
elif [[ "$TARGET" == "web" ]]; then
    cmake .. \
        -DBUILD_FOR_WEB=ON \
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
        fi
    elif [[ "$TARGET" == "web" ]]; then
        echo -e "${TURQ}        LAUNCHING LOCAL WEB SERVER — OPEN IN CHROME/EDGE${X}"
        echo -e "${AQUA}        http://localhost:8000/AMOURANTHRTX.html${X}"
        python3 -m http.server 8000 --directory "$BUILD_SUBDIR/bin/Web"
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
echo -e "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ ${TARGET^^} ${BUILD_VARIANT^^} BUILD COMPLETE ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo
echo -e "        ${W}Current Realm:${X} ${GLOW}$TARGET ${BUILD_VARIANT^^}${X}"
echo -e "        ${W}Binary Location:${X} ${GLOW}$FINAL_BINARY${X}"
echo -e "        ${W}Dive Command:${X}   ${AQUA}./linux.sh${TARGET:+$TARGET}${BUILD_VARIANT:+$BUILD_VARIANT} run${X}"
echo
echo -e "${GLOW}         — THE TIDE IS LOVE — ${X}"