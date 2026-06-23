#!/usr/bin/env bash
# seetests.sh — browse and run AMOURANTHRTX QA / audit / bench tests interactively.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
BIN_LINUX="$BUILD/bin/Linux"
SCRIPTS="$ROOT/scripts"

WHT='\033[1;97m'
GRN='\033[0;32m'
YLW='\033[1;33m'
RED='\033[0;31m'
CYN='\033[0;36m'
RST='\033[0m'

version() {
    python3 -c "import sys; sys.path.insert(0,'$SCRIPTS'); from ammo_platform import AMOURANTHRTX_VERSION; print(AMOURANTHRTX_VERSION)" 2>/dev/null || echo "?"
}

banner() {
    echo -e "${CYN}AMOURANTHRTX test toolkit${RST} — v$(version)"
    echo -e "build: ${WHT}$BUILD${RST}"
    echo
}

ensure_build_once() {
    if [[ -f "$BUILD/qa_amouranthos_test" ]]; then
        return 0
    fi
    echo -e "${YLW}Building QA targets (first run)…${RST}"
    cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)" || {
        echo -e "${RED}Build failed — run: ./linux.sh${RST}"
        return 1
    }
}

run_cmake_test() {
    local target="$1"
    ensure_build_once || return 1
    echo -e "${WHT}▶ cmake --build + $target${RST}"
    cmake --build "$BUILD" --target "$target" -j"$(nproc 2>/dev/null || echo 4)"
    local bin="$BUILD/$target"
    [[ -x "$bin" ]] || bin="$BIN_LINUX/$target"
    if [[ ! -x "$bin" ]]; then
        echo -e "${RED}Missing binary: $target${RST}"
        return 1
    fi
    (cd "$ROOT" && "$bin")
}

run_py_test() {
    local script="$1"
    shift || true
    echo -e "${WHT}▶ python3 scripts/$script $*${RST}"
    (cd "$ROOT" && python3 "$SCRIPTS/$script" "$@")
}

run_linux_sh() {
    local cmd="$1"
    shift || true
    echo -e "${WHT}▶ ./linux.sh $cmd $*${RST}"
    (cd "$ROOT" && ./linux.sh "$cmd" "$@")
}

# id|category|label|kind|payload
TESTS=(
  "1|AOS / GUI|AmouranthOS WM + taskbar|cmake|qa_amouranthos_test"
  "2|AOS / GUI|Taskbar click hits|cmake|qa_taskbar_click_test"
  "3|AOS / GUI|AOS GUI 4K + clock|cmake|qa_aos_gui_test"
  "4|AOS / GUI|Font SDF v2.02+|cmake|qa_font_sdf_test"
  "5|AOS / GUI|RTX widgets click|cmake|qa_rtx_widgets_test"
  "6|AOS / GUI|Exit shutdown dialog|cmake|qa_exit_shutdown_test"
  "7|AOS / GUI|App journal persist|cmake|qa_app_journal_test"
  "8|OCR / Visual|AOS OCR (Start/menu/NES/files)|py|qa_aos_ocr_test.py"
  "9|OCR / Visual|OCR click calibrate 4K|py|qa_ocr_click_test.py"
  "10|OCR / Visual|OCR toolkit only|py|ocr_click_toolkit.py"
  "11|OCR / Visual|NES OCR|py|qa_nes_ocr_test.py"
  "12|OCR / Visual|GUI startup|py|qa_gui_startup_test.py"
  "13|OCR / Visual|Startup smoke|py|qa_startup_test.py"
  "14|DOS / Input|Master DOS QA suite|py|qa_ammo_dos.py"
  "15|DOS / Input|DOS input|cmake|qa_dos_int_test"
  "16|DOS / Input|DOS commands|gpp|qa_dos_commands_test.cpp"
  "17|DOS / Input|CMD help|cmake|qa_cmd_help_test"
  "18|DOS / Input|DOS embed|cmake|qa_dos_embed_test"
  "19|DOS / Input|Registry|cmake|qa_registry_test"
  "20|DOS / Input|File index|cmake|qa_file_index_test"
  "21|DOS / Input|AmmoFAT|gpp|qa_ammofat_test.cpp"
  "22|DOS / Input|AmmoZIP|cmake|qa_ammozip_test"
  "23|DOS / Input|RTX VFS|cmake|qa_rtx_vfs_test"
  "24|DOS / Input|RTX PM|cmake|qa_rtxpm_test"
  "25|DOS / Input|RTX AmouranthOS shell|cmake|qa_rtx_ammos_test"
  "26|DOS / Input|Memory tiers|cmake|qa_memory_tier_test"
  "27|DOS / Input|Audio|cmake|qa_audio_test"
  "28|DOS / Input|Toolchain asm/link|cmake|qa_toolchain_test"
  "29|DOS / Input|Drivers build|cmake|qa_drivers_build"
  "30|Doom / Keen|Doom host|cmake|qa_doom_host_test"
  "31|Doom / Keen|Doom GPU|cmake|qa_doom_gpu_test"
  "32|Doom / Keen|Doom build|cmake|qa_doom_build_test"
  "33|Doom / Keen|Doom suite|py|qa_doom_test.py"
  "34|Doom / Keen|Doom viewport|py|qa_doom_viewport_test.py"
  "35|Doom / Keen|Keen host|cmake|qa_keen_host_test"
  "36|Doom / Keen|Keen build|cmake|qa_keen_build_test"
  "37|Emulators|NES CPU|cmake|qa_nes_cpu_test"
  "38|Emulators|SNES|cmake|qa_snes_test"
  "39|Emulators|PS1|cmake|qa_ps1_test"
  "40|Emulators|N64|cmake|qa_n64_test"
  "41|Emulators|Dreamcast|cmake|qa_dreamcast_test"
  "42|Emulators|PS2|cmake|qa_ps2_test"
  "43|Emulators|Xbox360|cmake|qa_xbox360_test"
  "44|Emulators|Amiga|cmake|qa_amiga_test"
  "45|Field / Storage|Field persist|cmake|qa_field_persist_test"
  "46|Field / Storage|FieldStorage|cmake|qa_fieldstorage_test"
  "47|Field / Storage|Field Kilroy|cmake|qa_field_kilroy_test"
  "48|Field / Storage|RTX density|cmake|qa_rtx_density_test"
  "49|Field / Storage|RAID|cmake|qa_raid_test"
  "50|Field / Storage|Bench storage|py|bench_storage.py"
  "51|Field / Storage|Team drive|py|team_drive_test.py"
  "52|Everything|Everything everywhere|cmake|qa_everything_test"
  "53|Everything|Love demo|cmake|qa_love_demo_test"
  "54|Audits|Monolith audit|py|monolith_audit.py"
  "55|Audits|Zero-cost audit|py|zero_cost_audit.py --end-game"
  "56|Audits|End-game audit|py|end_game_audit.py"
  "57|Audits|Bench MAME compare|py|bench_mame_compare.py"
  "58|Audits|Bench DOS suite|py|bench_dos_suite.py"
  "59|Audits|Play legacy|py|play_legacy.py"
  "60|Release|Full release checklist|py|release_checklist_2_0.py"
  "61|Release|Publish GitHub release|sh|publish_github_release.sh"
)

run_gpp_standalone() {
    local src="$1"
    echo -e "${WHT}▶ g++ standalone $src${RST}"
    (cd "$ROOT" && python3 -c "
import subprocess, sys
from pathlib import Path
root = Path('$ROOT')
sys.path.insert(0, str(root/'scripts'))
# reuse qa_ammo_dos compile helpers
import qa_ammo_dos as q
out = q.compile_standalone('$src')
subprocess.run([str(out)], cwd=root, check=False)
")
}

dispatch() {
    local kind="$1" payload="$2"
    case "$kind" in
        cmake) run_cmake_test "$payload" ;;
        py)    run_py_test "$payload" ;;
        sh)    bash "$SCRIPTS/$payload" ;;
        gpp)   run_gpp_standalone "$payload" ;;
        linux) run_linux_sh "$payload" ;;
        *)     echo "Unknown kind: $kind"; return 1 ;;
    esac
}

list_by_category() {
    local cat="${1:-}"
    local i=0
    for entry in "${TESTS[@]}"; do
        IFS='|' read -r id category label kind payload <<< "$entry"
        if [[ -z "$cat" || "$category" == "$cat" ]]; then
            printf "  %2s  %-22s  %s\n" "$id" "[$category]" "$label"
            ((i++)) || true
        fi
    done
    echo
    echo -e "${GRN}$i tests listed${RST}"
}

categories() {
    printf '%s\n' "${TESTS[@]}" | awk -F'|' '{print $2}' | sort -u
}

pick_and_run() {
    local choice="${1:-}"
    if [[ -z "$choice" ]]; then
        read -r -p "Test id (or q): " choice
    fi
    [[ "$choice" == "q" || -z "$choice" ]] && return 0
    for entry in "${TESTS[@]}"; do
        IFS='|' read -r id category label kind payload <<< "$entry"
        if [[ "$id" == "$choice" ]]; then
            echo -e "\n${CYN}[$category] $label${RST}\n"
            dispatch "$kind" "$payload"
            return $?
        fi
    done
    echo -e "${RED}Unknown id: $choice${RST}"
    return 1
}

run_category() {
    local cat="$1"
    local failed=0 passed=0
    for entry in "${TESTS[@]}"; do
        IFS='|' read -r id category label kind payload <<< "$entry"
        [[ "$category" == "$cat" ]] || continue
        echo -e "\n${CYN}━━ [$category] $label ━━${RST}"
        if dispatch "$kind" "$payload"; then
            ((passed++)) || true
        else
            ((failed++)) || true
            echo -e "${RED}FAILED: $label${RST}"
        fi
    done
    echo -e "\n${GRN}Category $cat: $passed passed, $failed failed${RST}"
    [[ "$failed" -eq 0 ]]
}

interactive() {
    while true; do
        banner
        echo "  l) List all tests"
        echo "  c) List categories"
        echo "  r) Run test by id"
        echo "  g) Run category group"
        echo "  a) Run AOS/GUI + OCR suite (quick)"
        echo "  q) Quit"
        echo
        read -r -p "> " cmd
        case "${cmd,,}" in
            l|list)
                echo
                list_by_category
                read -r -p "Press Enter…" _
                ;;
            c|cat*)
                echo
                categories | nl
                echo
                read -r -p "Category name: " cat
                list_by_category "$cat"
                read -r -p "Press Enter…" _
                ;;
            r|run*)
                pick_and_run
                read -r -p "Press Enter…" _
                ;;
            g|group*)
                categories | nl
                read -r -p "Category name: " cat
                run_category "$cat" || true
                read -r -p "Press Enter…" _
                ;;
            a|aos*)
                run_category "AOS / GUI" || true
                run_category "OCR / Visual" || true
                read -r -p "Press Enter…" _
                ;;
            q|quit|exit) break ;;
            [0-9]*)
                pick_and_run "$cmd"
                read -r -p "Press Enter…" _
                ;;
            *) echo "?" ;;
        esac
    done
}

usage() {
    echo "Usage: $0 [list|categories|run ID|category NAME|aos|interactive]"
    echo "  ./seetests.sh           — interactive browser"
    echo "  ./seetests.sh list      — print all tests"
    echo "  ./seetests.sh run 3     — run test id 3"
    echo "  ./seetests.sh category 'AOS / GUI'"
    echo "  ./seetests.sh aos       — AOS + OCR quick suite"
}

main() {
    chmod +x "$0" 2>/dev/null || true
    case "${1:-interactive}" in
        list|ls)     banner; list_by_category ;;
        categories)  banner; categories ;;
        run)         shift; banner; pick_and_run "${1:-}" ;;
        category|cat) shift; banner; run_category "${1:-}" ;;
        aos|quick)   banner; run_category "AOS / GUI"; run_category "OCR / Visual" ;;
        help|-h|--help) usage ;;
        interactive|i|"") interactive ;;
        [0-9]*)      banner; pick_and_run "$1" ;;
        *)           usage; exit 1 ;;
    esac
}

main "$@"