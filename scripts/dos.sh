#!/usr/bin/env bash
# =============================================================================
# AMOURANTHRTX — RTX-DOS 7.0 GPU Super DOSBox (Microsoft MS-DOS MIT lineage)
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALLER="$ROOT/scripts/install_dos.py"

RED='\033[1;31m'
GRN='\033[1;32m'
CYN='\033[1;36m'
WHT='\033[1;97m'
RST='\033[0m'

banner() {
    echo -e "${CYN}"
    echo "  ╔══════════════════════════════════════════════════════════╗"
    echo "  ║  AMOURANTHRTX · RTX-DOS 7.0 GPU Super DOSBox — Field Die ║"
    echo "  ╚══════════════════════════════════════════════════════════╝"
    echo -e "${RST}"
}

usage() {
    cat <<EOF
Usage: ./linux.sh dos [options]

  (default)     Build RTX-DOS 7.0 from Microsoft MS-DOS MIT source
  --force, -f   Rebuild all images
  --verify      Print install manifest

Lineage: github.com/microsoft/MS-DOS (v4.0 MIT) -> RTX-DOS 7.0 GPU patch

Assets:
  rtx_dos720.img   720 KiB MS-DOS MIT boot floppy (IO.SYS -> C:\\COMMAND.COM)
  rtx_dos_hd.img   512 MiB C: RTXDOS (embedded 32 MiB slice in engine)
  assets/dos/ammo/ Patched RTX-DOS 7.0 tree (rebuild: scripts/rtx_dos/)

./linux.sh run — RTX-DOS panel (Home = postage stamp / fullscreen zoom)
EOF
}

main() {
    banner
    need_cmd() { command -v "$1" >/dev/null || { echo -e "${RED}Missing: $1${RST}"; exit 1; }; }
    need_cmd python3
    need_cmd git

    case "${1:-}" in
        --help|-h|help) usage; exit 0 ;;
        --verify)
            [[ -f "$ROOT/assets/dos/install_manifest.json" ]] && cat "$ROOT/assets/dos/install_manifest.json" \
                || { echo -e "${RED}Not installed${RST}"; exit 1; }
            exit 0 ;;
    esac

    echo -e "${WHT}RTX-DOS 7.0 — Microsoft MS-DOS 4.0 MIT -> GPU Super DOSBox${RST}"

    ensure_cmake_build() {
        local build="$ROOT/build"
        if [[ ! -f "$build/CMakeCache.txt" ]]; then
            echo -e "${WHT}  cmake configure (first-time build dir)${RST}"
            mkdir -p "$build"
            cmake -S "$ROOT" -B "$build" -DCMAKE_BUILD_TYPE=Debug
        fi
        echo -e "${WHT}  cmake --build field_x86 qa_drivers_build${RST}"
        cmake --build "$build" --target field_x86 qa_drivers_build -j"$(nproc 2>/dev/null || echo 4)"
    }
    ensure_cmake_build

    python3 "$ROOT/scripts/gen_rtx_font.py"
    python3 "$ROOT/scripts/gen_rtx_sdf_font.py"
    python3 "$INSTALLER" "$@"
    echo -e "${GRN}Done.${RST} Run ${WHT}./linux.sh run${RST}"
}

main "$@"