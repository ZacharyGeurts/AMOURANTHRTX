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

# Doom is a separate Chocolate Doom + FreeDoom launch (not the GPU x86 die).
for arg in "$@"; do
    case "${arg,,}" in
        doom)
            chmod +x "$PROJECT_ROOT/scripts/doom.sh" 2>/dev/null || true
            exec "$PROJECT_ROOT/scripts/doom.sh" "${@:2}"
            ;;
        dos)
            chmod +x "$PROJECT_ROOT/scripts/dos.sh" 2>/dev/null || true
            exec "$PROJECT_ROOT/scripts/dos.sh" "${@:2}"
            ;;
        qa)
            exec python3 "$PROJECT_ROOT/scripts/qa_ammo_dos.py"
            ;;
        bench)
            exec python3 "$PROJECT_ROOT/scripts/bench_dos_suite.py" "${@:2}"
            ;;
        team-drive|team_drive)
            exec python3 "$PROJECT_ROOT/scripts/team_drive_test.py" "${@:2}"
            ;;
        brain|field-brain|field_brain)
            exec python3 "$PROJECT_ROOT/scripts/field_brain_memory.py" "${@:2}"
            ;;
        super|superintel|field-super)
            exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" "${@:2}"
            ;;
        hostess|hostess7|ceo|leadership)
            if [[ $# -lt 2 ]]; then
                exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" hostess
            else
                exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" "${@:2}"
            fi
            ;;
        lead|ceo-session)
            exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" lead "${@:2}"
            ;;
        batch|fix-batch)
            exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" batch "${@:2}"
            ;;
        turnover|turn-over|phase0)
            exec python3 "$PROJECT_ROOT/scripts/field_superintelligence.py" turnover "${@:2}"
            ;;
        month-targets|month_targets)
            exec python3 "$PROJECT_ROOT/scripts/month_targets.py" "${@:2}"
            ;;
        field-storage|field_storage)
            exec python3 "$PROJECT_ROOT/scripts/bench_storage.py" "${@:2}"
            ;;
        end-game|end_game)
            exec python3 "$PROJECT_ROOT/scripts/end_game_audit.py" "${@:2}"
            ;;
        release-2.0|release_2_0|release)
            exec python3 "$PROJECT_ROOT/scripts/release_checklist_2_0.py" "${@:2}"
            ;;
        seetests|tests|test-menu)
            chmod +x "$PROJECT_ROOT/seetests.sh" 2>/dev/null || true
            exec "$PROJECT_ROOT/seetests.sh" "${@:2}"
            ;;
        publish-release|publish_release|github-release)
            chmod +x "$PROJECT_ROOT/scripts/publish_github_release.sh" 2>/dev/null || true
            exec "$PROJECT_ROOT/scripts/publish_github_release.sh" "${@:2}"
            ;;
        win31)
            exec python3 "$PROJECT_ROOT/scripts/setup_win31.py" "${@:2}"
            ;;
        os|kilroy|field)
            chmod +x "$PROJECT_ROOT/scripts/build_kilroy_os.sh" \
                     "$PROJECT_ROOT/scripts/field_kilroy.sh" 2>/dev/null || true
            exec "$PROJECT_ROOT/scripts/build_kilroy_os.sh" "${@:2}"
            ;;
    esac
done

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
║          GOLDEN ERA MAN+MACHINE — AQUA TEMPLE — FOREVER PLACE                ║
╚══════════════════════════════════════════════════════════════════════════════╝

  Golden Era Man+Machine — human intent meets silicon on the Field Die.
  Host binary: AMOURANTHRTX -h | /h | --help | /help

  ./linux.sh                  → build debug Linux (default)
  ./linux.sh run              → build + launch debug Linux
  ./linux.sh run --extended-field
                              → extended non-point wave dispatch (+35-60% throughput)
  ./linux.sh release          → build release Linux
  ./linux.sh windows          → cross-compile Windows (release)
  ./linux.sh web              → build WebAssembly (Emscripten + WebGPU)
  ./linux.sh web run          → build web + launch local server (open in browser)
  ./linux.sh single           → -j1 build (current target)
  ./linux.sh gdb              → launch under gdb (Linux only)
  ./linux.sh doom             → download FreeDoom + play via Chocolate Doom
  ./linux.sh dos              → install RTX-DOS 7.0 GPU Super DOSBox on C:
  ./linux.sh dos --force      → re-download and rebuild RTX-DOS images
  ./linux.sh qa               → full DOS/Windows QA suite
  ./linux.sh bench            → CPU/GPU perf (add --dosbox for DOSBox ref, --quick to skip extras)
  ./linux.sh team-drive       → TEAM empty nvme2n1 harness (non-destructive)
  ./linux.sh field-storage    → FieldStorage v2 bench + multi-FS QA
  ./linux.sh run --dual --dual-host --team-drive --field-storage-v2
                              → dual Linux/Windows + TEAM + FieldStorage v2 flags
  ./linux.sh run --infinite --chips --chips-ps1 --field-emulator
                              → infinite SDF wave + CHIPS PS1 GPU die emulation
  ./linux.sh run --chips-all --amiga-love --xbox360
                              → Love of EVERYTHING: PS1+N64 tier+Xbox360+Amiga canvas
  ./linux.sh run --all-breakthroughs --extended-field --infinite
                              → hyper physics 1-4 + phi-thermo fabric + infinite SDF
  ./linux.sh run --end-game
                              → END OF EVERYTHING: infinite + dual + TEAM + CHIPS-all + hyper physics
  ./linux.sh end-game           → full end-game QA matrix (fast, skips doom)
  ./linux.sh win31            → Windows 3.1 MCSE+I setup checklist
  ./linux.sh win31 --stage    → stage incoming/win31 + rebuild C:
  ./linux.sh os               → full KILROY Field OS (engine + kernel + rootfs + boot ISO)
  ./linux.sh os qemu          → boot KILROY in QEMU
  ./linux.sh os betatest      → betatester Linux command suite
  ./linux.sh kilroy qa        → FieldKilroy syscall router QA
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
    make -C "$PROJECT_ROOT/Navigator/shaders" clean 2>/dev/null || true
    
    echo -e "${GLOW}        ALL BUILD REALMS PURGED — FRESH OCEAN AWAITS${X}"
    exit 0
}

# ── ARGUMENT PARSING ────────────────────────────────────────────────────────
ACTION="build"
BUILD_JOBS="$(nproc)"
GENERATOR="Unix Makefiles"
LAUNCH_MODE="normal"
WINE_RUN=false
EXTENDED_FIELD=false
DUAL_HOST=false
TEAM_DRIVE=false
FIELD_STORAGE_V2=false
INFINITE_SDF=false
CHIPS_EXPANSION=false
CHIPS_PS1=false
CHIPS_ALL=false
AMIGA_LOVE=false
XBOX360=false
ALL_BREAKTHROUGHS=false
FIELD_EMULATOR=false
END_GAME=false
EVERYTHING_EVERYWHERE=false
FIELD_PERSIST=false

for arg in "$@"; do
    case "${arg,,}" in
        run)        ACTION="run" ;;
        single)     ACTION="run"; BUILD_JOBS="1" ;;
        gdb)        ACTION="run"; LAUNCH_MODE="gdb" ;;
        clean)      clean_all ;;
        ninja|--ninja) GENERATOR="Ninja" ;;
        extended-field|--extended-field) EXTENDED_FIELD=true; ALL_BREAKTHROUGHS=true ;;
        all-breakthroughs|--all-breakthroughs) ALL_BREAKTHROUGHS=true; EXTENDED_FIELD=true ;;
        dual|--dual|dual-host|--dual-host) DUAL_HOST=true ;;
        team-drive|--team-drive) TEAM_DRIVE=true ;;
        field-storage|--field-storage|field-storage-v2|--field-storage-v2) FIELD_STORAGE_V2=true ;;
        infinite|--infinite) INFINITE_SDF=true ;;
        chips|--chips) CHIPS_EXPANSION=true ;;
        chips-ps1|--chips-ps1) CHIPS_PS1=true; CHIPS_EXPANSION=true ;;
        chips-all|--chips-all) CHIPS_ALL=true; CHIPS_EXPANSION=true; CHIPS_PS1=true; AMIGA_LOVE=true; XBOX360=true ;;
        amiga-love|--amiga-love) AMIGA_LOVE=true; CHIPS_EXPANSION=true ;;
        xbox360|--xbox360) XBOX360=true; CHIPS_EXPANSION=true ;;
        field-emulator|--field-emulator) FIELD_EMULATOR=true ;;
        end-game|--end-game) END_GAME=true ;;
        everything-everywhere|--everything-everywhere) END_GAME=true; EVERYTHING_EVERYWHERE=true; FIELD_PERSIST=true ;;
        field-persist|--field-persist) FIELD_PERSIST=true ;;
        windows|release|web) ;; # already handled
        --help|-h|/h|/help|-help|help|"") show_help ;;
        *)          echo -e "${CORAL}UNKNOWN CURRENT: $arg${X}"; show_help ;;
    esac
done

if $EXTENDED_FIELD; then
    export AMOURANTHRTX_EXTENDED_FIELD=1
fi
if $DUAL_HOST; then
    export AMOURANTHRTX_DUAL_HOST=1
fi
if $TEAM_DRIVE; then
    export TEAM_DRIVE_DEV="${TEAM_DRIVE_DEV:-/dev/nvme2n1}"
    export AMOURANTHRTX_TEAM_DRIVE=1
fi
if $FIELD_STORAGE_V2; then
    export AMOURANTHRTX_FIELD_STORAGE_V2=1
fi
if $INFINITE_SDF; then
    export AMOURANTHRTX_INFINITE=1
fi
if $CHIPS_EXPANSION; then
    export AMOURANTHRTX_CHIPS=1
fi
if $CHIPS_PS1; then
    export AMOURANTHRTX_CHIPS_PS1=1
fi
if $FIELD_EMULATOR; then
    export AMOURANTHRTX_FIELD_EMULATOR=1
fi
if $CHIPS_ALL; then
    export AMOURANTHRTX_CHIPS_ALL=1
fi
if $AMIGA_LOVE; then
    export AMOURANTHRTX_AMIGA_LOVE=1
fi
if $XBOX360; then
    export AMOURANTHRTX_XBOX360=1
fi
if $ALL_BREAKTHROUGHS; then
    export AMOURANTHRTX_ALL_BREAKTHROUGHS=1
fi
if $CHIPS_ALL || $EXTENDED_FIELD; then
    export AMOURANTHRTX_CHIPS_FABRIC=1
fi
if $END_GAME; then
    export AMOURANTHRTX_END_GAME=1
    export AMOURANTHRTX_INFINITE=1
    export AMOURANTHRTX_DUAL_HOST=1
    export AMOURANTHRTX_TEAM_DRIVE=1
    export AMOURANTHRTX_FIELD_STORAGE_V2=1
    export AMOURANTHRTX_CHIPS=1
    export AMOURANTHRTX_CHIPS_PS1=1
    export AMOURANTHRTX_CHIPS_ALL=1
    export AMOURANTHRTX_AMIGA_LOVE=1
    export AMOURANTHRTX_XBOX360=1
    export AMOURANTHRTX_ALL_BREAKTHROUGHS=1
    export AMOURANTHRTX_EXTENDED_FIELD=1
    export AMOURANTHRTX_CHIPS_FABRIC=1
    export AMOURANTHRTX_FIELD_EMULATOR=1
    export TEAM_DRIVE_DEV="${TEAM_DRIVE_DEV:-/dev/nvme2n1}"
fi
if $EVERYTHING_EVERYWHERE; then
    export AMOURANTHRTX_EVERYTHING_EVERYWHERE=1
    export AMOURANTHRTX_FIELD_PERSIST=1
fi
if $FIELD_PERSIST; then
    export AMOURANTHRTX_FIELD_PERSIST=1
fi

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

echo -e "${AQUA}        Generating AmouranthOS RTX icon atlas${X}"
python3 "$PROJECT_ROOT/scripts/gen_amouranthos_icons.py"
if [[ "$TARGET" == "linux" ]]; then
    python3 "$PROJECT_ROOT/scripts/detect_browsers.py" 2>/dev/null || true
fi
echo -e "${AQUA}        x86 comp compiler — CANVAS + DOS viewport + field themes${X}"
python3 "$PROJECT_ROOT/Navigator/shaders/compute/sync_canvas_shaders.py"
echo -e "${AQUA}        SPIR-V shaders — Makefile (glslc → assets/shaders/)${X}"
make -C "$PROJECT_ROOT/Navigator/shaders" BUILD_TYPE="${BUILD_VARIANT^}" -j"$BUILD_JOBS"

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

# Warm Vulkan pipeline cache (x86 compile ~3s with cache vs ~10min cold).
CACHE_SRC="$PROJECT_ROOT/cache/vulkan_compute.cache"
CACHE_DST="$PROJECT_ROOT/build/bin/Linux/cache/vulkan_compute.cache"
if [[ -f "$CACHE_SRC" ]]; then
    mkdir -p "$(dirname "$CACHE_DST")"
    cp -f "$CACHE_SRC" "$CACHE_DST"
    echo -e "${GLOW}        Vulkan pipeline cache ready — ${X}$(du -h "$CACHE_SRC" | cut -f1)"
fi

# ── BINARY ASCENSION ───────────────────────────────────────────────────────
[[ ! -f "$SOURCE_BINARY" ]] && { echo -e "${CORAL}        FATAL: Binary drowned — $SOURCE_BINARY${X}"; exit 1; }

mkdir -p "$(dirname "$FINAL_BINARY")"
echo -e "${GLOW}        BINARY SURFACED → $FINAL_BINARY${X}"

# ── LAUNCH CEREMONY ────────────────────────────────────────────────────────
if [[ "$ACTION" == "run" ]]; then
    echo
    echo -e "${PEARL}       THROUGH WATER — LAUNCHING IN ${TARGET^^} ${BUILD_VARIANT^^} REALM${X}"
    if $EXTENDED_FIELD; then
        echo -e "${GLOW}        EXTENDED FIELD — non-point wave dispatch active (+35-60% throughput)${X}"
    fi
    echo -e "${GLOW}        The ocean crosses dimensions. Dive deep.${X}"
    echo

    cd "$PROJECT_ROOT"

    if [[ ! -f "$PROJECT_ROOT/assets/dos/rtx_dos720.img" ]] \
       || [[ ! -f "$PROJECT_ROOT/assets/dos/rtx_dos_hd.img" ]]; then
        echo -e "${WAVE}        RTX-DOS not installed — running ./linux.sh dos${X}"
        chmod +x "$PROJECT_ROOT/scripts/dos.sh" 2>/dev/null || true
        "$PROJECT_ROOT/scripts/dos.sh"
    fi

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
        RUN_ARGS=()
        $EXTENDED_FIELD && RUN_ARGS+=(--extended-field)
        for arg in "${@:2}"; do
            case "${arg,,}" in
                extended-field|--extended-field) ;;
                *) RUN_ARGS+=("$arg") ;;
            esac
        done
        gdb -ex run --args "$FINAL_BINARY" "${RUN_ARGS[@]}"
    else
        RUN_ARGS=()
        $EXTENDED_FIELD && RUN_ARGS+=(--extended-field)
        for arg in "${@:2}"; do
            case "${arg,,}" in
                extended-field|--extended-field) ;;
                *) RUN_ARGS+=("$arg") ;;
            esac
        done
        exec "$FINAL_BINARY" "${RUN_ARGS[@]}"
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
if [[ "$TARGET" == "linux" && -f "$PROJECT_ROOT/assets/AMOURANTHRTX.desktop" ]]; then
    DESKTOP_DIR="$HOME/.local/share/applications"
    mkdir -p "$DESKTOP_DIR"
    sed -e "s|AMOURANTHRTX_BIN|$FINAL_BINARY|g" \
        -e "s|AMOURANTHRTX_ROOT|$PROJECT_ROOT|g" \
        "$PROJECT_ROOT/assets/AMOURANTHRTX.desktop" > "$DESKTOP_DIR/amouranthrtx.desktop"
    chmod +x "$DESKTOP_DIR/amouranthrtx.desktop" 2>/dev/null || true
    echo -e "        ${W}Mint launcher:${X}   ${GLOW}$DESKTOP_DIR/amouranthrtx.desktop${X}"
fi
echo
echo -e "${GLOW}         — THE TIDE IS LOVE — ${X}"