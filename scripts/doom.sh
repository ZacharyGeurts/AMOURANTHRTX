#!/usr/bin/env bash
# =============================================================================
# AMOURANTHRTX — FreeDoom launcher (Chocolate Doom)
# The GPU Field Die is a debug HUD, not a DOS PC. Real Doom runs here.
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WAD_DIR="$ROOT/assets/wads"
WAD="$WAD_DIR/freedoom1.wad"
FREEDOOM_URL="https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip"

RED='\033[1;31m'
GRN='\033[1;32m'
CYN='\033[1;36m'
WHT='\033[1;97m'
RST='\033[0m'

banner() {
    echo -e "${CYN}"
    echo "  ╔══════════════════════════════════════════════════════════╗"
    echo "  ║  AMOURANTHRTX · DOOM — FreeDoom + Chocolate Doom         ║"
    echo "  ╚══════════════════════════════════════════════════════════╝"
    echo -e "${RST}"
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo -e "${RED}Missing required tool: $1${RST}"
        exit 1
    }
}

ensure_wad() {
    mkdir -p "$WAD_DIR"
    if [[ -f "$WAD" ]]; then
        echo -e "${GRN}IWAD ready:${RST} $WAD"
        return
    fi
    need_cmd curl
    need_cmd unzip
    echo -e "${WHT}Downloading FreeDoom 0.13.0 (GPL)…${RST}"
    local tmp zip
    tmp="$(mktemp -d)"
    zip="$tmp/freedoom.zip"
    curl -fsSL -o "$zip" "$FREEDOOM_URL"
    unzip -qo "$zip" -d "$tmp/extract"
    find "$tmp/extract" -name 'freedoom1.wad' -exec cp -f {} "$WAD" \;
    rm -rf "$tmp"
    [[ -f "$WAD" ]] || { echo -e "${RED}freedoom1.wad not found in archive${RST}"; exit 1; }
    echo -e "${GRN}Installed:${RST} $WAD"
}

find_doom_engine() {
    local c path
    local -a candidates=(
        "$ROOT/third_party/chocolate-doom/build/src/chocolate-doom"
        "$ROOT/third_party/chocolate-doom/install/bin/chocolate-doom"
        chocolate-doom
        chocolate-doom-game
    )
    for c in "${candidates[@]}"; do
        if [[ -x "$c" ]]; then
            echo "$c"
            return 0
        fi
        if path="$(command -v "$c" 2>/dev/null)"; then
            echo "$path"
            return 0
        fi
    done
    return 1
}

ensure_engine() {
    local engine
    if engine="$(find_doom_engine)"; then
        echo -e "${GRN}Engine:${RST} $engine"
        DOOM_ENGINE="$engine"
        return
    fi
    echo -e "${WHT}Chocolate Doom not found — building local copy (one-time)…${RST}"
    need_cmd cmake
    need_cmd git
    local src="$ROOT/third_party/chocolate-doom"
    if [[ ! -d "$src/.git" ]]; then
        mkdir -p "$ROOT/third_party"
        git clone --depth 1 https://github.com/chocolate-doom/chocolate-doom.git "$src"
    fi
    cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$src/build" -j"$(nproc)"
    DOOM_ENGINE="$(find_doom_engine)" || {
        echo -e "${RED}Chocolate Doom build failed.${RST}"
        echo "Install system package: sudo apt install chocolate-doom"
        exit 1
    }
    echo -e "${GRN}Engine:${RST} $DOOM_ENGINE"
}

main() {
    banner
    echo -e "${CYN}Note:${RST} In-engine Shareware Doom: ./linux.sh dos then ./linux.sh run"
    echo "      Boot RTX-DOS in the Field Die panel, type ${WHT}DOOM${RST} at C:\\>"
    echo "      This launcher is external Chocolate Doom + FreeDoom for side-by-side play."
    echo
    ensure_wad
    if command -v python3 >/dev/null 2>&1; then
        python3 "$ROOT/scripts/extract_doom_gpu.py" || true
    fi
    ensure_engine
    echo
    echo -e "${WHT}Launching Doom. Controls: arrows move, Ctrl fire, Space use/open, Esc menu.${RST}"
    echo -e "${WHT}Pass extra args to chocolate-doom (e.g. -skill 4 -warp 1).${RST}"
    echo
    exec "$DOOM_ENGINE" -iwad "$WAD" "$@"
}

main "$@"