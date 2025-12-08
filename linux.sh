#!/usr/bin/env bash
# =============================================================================
# linux.sh — AMOURANTH RTX — WATER TEMPLE EDITION — CROSS REALMS ETERNAL
# Works perfectly with:  ./linux.sh | windows | run | single | gdb | ninja | clean
# BINDING 33 — AQUAMARINE PHOTONS FLOW FLAWLESSLY — DECEMBER 08, 2025
# =============================================================================

set -euo pipefail

# ── OCEAN PALETTE ────────────────────────────────────────────────────────────
AQUA="\033[38;5;51m"   DEEP="\033[38;5;27m"   TURQ="\033[38;5;45m"
WAVE="\033[38;5;39m"   FOAM="\033[38;5;195m"  PEARL="\033[38;5;231m"
CORAL="\033[38;5;204m" ABYSS="\033[38;5;17m"  GLOW="\033[38;5;159m"
W="\033[1;97m"        X="\033[0m"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

banner() {
    clear
    echo -e "${DEEP}  █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    echo -e "${AQUA} ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    echo -e "${TURQ} ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    echo -e "${WAVE} ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    echo -e "${GLOW} ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    echo -e "${FOAM} ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    echo
    echo -e "${CORAL}  ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    echo -e "${AQUA}  ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    echo -e "${TURQ}  ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    echo -e "${WAVE}  ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    echo -e "${GLOW}  ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    echo -e "${PEARL}  ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    echo
    echo -e "${GLOW}            AMOURANTH RTX — WATER TEMPLE — $(date '+%B %d, %Y')${X}"
    echo -e "${W}            BINDING 33 ACTIVE — THE TIDE IS PERFECT — GROK BLESSED THIS${X}"
    echo
}

# ── DEFAULTS ────────────────────────────────────────────────────────────────
TARGET="linux"
ACTION="build"
JOBS="$(nproc)"
GENERATOR="Unix Makefiles"
RUN_WITH="normal"   # normal | gdb
BUILD_DIR="build"

[[ " $* " =~ " windows " ]] && TARGET="windows"
[[ " $* " =~ " run "     ]] && ACTION="run"
[[ " $* " =~ " single "  ]] && JOBS=1 && ACTION="run"
[[ " $* " =~ " gdb "     ]] && RUN_WITH="gdb" && ACTION="run"
[[ " $* " =~ " ninja "   ]] && GENERATOR="Ninja"
[[ " $* " =~ " clean "   ]] && { rm -rf build build-windows; banner; echo -e "${GLOW}        BOTH REALMS PURGED — THE ABYSS IS CLEAN${X}"; exit 0; }
[[ " $* " =~ " -h " || " $* " =~ " help " || -z "$*" ]] && { banner; cat "$0" | grep -A30 "show_help()" | tail -n +3 | sed -n '/EOF/q;p' | sed 's/.*EOF//'; exit 0; }

# ── PATHS ───────────────────────────────────────────────────────────────────
if [[ $TARGET == "windows" ]]; then
    BUILD_DIR="build-windows"
    BINARY="$ROOT/$BUILD_DIR/bin/Windows/Navigator.exe"
else
    BINARY="$ROOT/build/bin/Linux/Navigator"
fi

# ── BUILD ───────────────────────────────────────────────────────────────────
banner
echo -e "${WAVE}        SURFACING IN ${TARGET^^} REALM — $GENERATOR — $JOBS threads${X}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# TOOLCHAIN FIRST, GENERATOR LAST → this is the only correct order
if [[ $TARGET == "windows" ]]; then
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/Toolchain-mingw64.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "$GENERATOR"
else
    cmake .. \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "$GENERATOR"
fi

echo -e "${AQUA}        FORGING AQUAMARINE PHOTONS — $JOBS threads${X}"
cmake --build . -j"$JOBS"

# ── RUN ─────────────────────────────────────────────────────────────────────
if [[ $ACTION == "run" ]]; then
    [[ ! -f "$BINARY" ]] && { echo -e "${CORAL}        FATAL: No Navigator found — build first!${X}; exit 1; }

    echo -e "${PEARL}        FIRST LIGHT — LAUNCHING IN ${TARGET^^} REALM${X}"
    cd "$ROOT"

    if [[ $TARGET == "windows" ]]; then
        if command -v wine64 &>/dev/null; then
            echo -e "${WAVE}        CROSSING DIMENSIONS THROUGH WINE...${X}"
            wine64 "$BINARY" "$@"
        else
            echo -e "${CORAL}        Wine not found — copy Navigator.exe to Windows${X}"
        fi
    elif [[ $RUN_WITH == "gdb" ]]; then
        echo -e "${WAVE}        DIVING WITH GDB — MAY YOUR SOUL BE DEBUGGED${X}"
        gdb -ex run --args "$BINARY" "$@"
    else
        exec "$BINARY" "$@"
    fi
fi

# ── FINAL TIDE ─────────────────────────────────────────────────────────────
banner
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo -e "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ ${TARGET^^} BUILD COMPLETE — NAVIGATOR ASCENDS ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
echo -e "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
echo -e "        ${W}Realm:${X}   ${GLOW}$TARGET${X}"
echo -e "        ${W}Binary:${X}  ${GLOW}$BINARY${X}"
echo -e "        ${W}Dive:${X}    ${AQUA}./linux.sh $TARGET run${X}"
echo
echo -e "${GLOW}        AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE — GROK WAS HERE${X}"