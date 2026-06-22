#!/usr/bin/env bash
# 20-second headless boot smoke test — RTX-DOS / Field Die init only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin/Linux/AMOURANTHRTX"
LOG="$ROOT/build/debug_boot_20s.log"
WALL_SEC=20
MAX_FRAMES=24

EXTENDED=0
for arg in "$@"; do
    case "${arg,,}" in
        --extended-field|extended-field) EXTENDED=1 ;;
    esac
done

if [[ ! -x "$BIN" ]]; then
    echo "FATAL: missing $BIN — run: ./linux.sh"
    exit 1
fi

mkdir -p "$(dirname "$LOG")"
: >"$LOG"

ARGS=()
[[ $EXTENDED -eq 1 ]] && ARGS+=(--extended-field)

echo "=== debug_boot_20s: ${WALL_SEC}s wall, MAX_FRAMES=$MAX_FRAMES ==="
echo "log: $LOG"

set +e
timeout --signal=TERM --kill-after=3 "$WALL_SEC" \
    env AMOURANTHRTX_HEADLESS=1 \
        AMOURANTHRTX_MAX_FRAMES="$MAX_FRAMES" \
        AMOURANTHRTX_EXTENDED_FIELD="$EXTENDED" \
        AMOURANTHRTX_BENCH_W=1280 \
        AMOURANTHRTX_BENCH_H=720 \
        AMOURANTHRTX_NO_VALIDATION=1 \
        VK_INSTANCE_LAYERS= \
    "$BIN" "${ARGS[@]}" >>"$LOG" 2>&1
RC=$?
set -e

# 124 = timeout wall; 137 = SIGKILL after TERM (Vk cold compile may ignore TERM)
# 0 = clean exit before wall
if [[ $RC -ne 0 && $RC -ne 124 && $RC -ne 137 ]]; then
    echo "FAIL: exit $RC"
    tail -40 "$LOG"
    exit "$RC"
fi

PASS=0
grep -q "Apocalypse handler" "$LOG" && PASS=$((PASS + 1))
grep -qE "RayCanvas|CTOR: begin RayCanvas" "$LOG" && PASS=$((PASS + 1))
grep -qE "aos_load|Headless bounded|Field pipeline|ACCOUNTANT THERMO" "$LOG" && PASS=$((PASS + 1))
grep -qE "MAX_FRAMES|Engine shutdown|Canvas destroyed|HEADLESS RayCanvas ready" "$LOG" && PASS=$((PASS + 1))

echo "--- boot markers: $PASS/4 ---"
grep -E "Apocalypse|RayCanvas|aos_load|RTX-DOS|MAX_FRAMES|shutdown|EXTENDED|extended" "$LOG" | tail -20 || true

if [[ $PASS -lt 3 ]]; then
    echo "FAIL: boot markers insufficient ($PASS/4)"
    tail -60 "$LOG"
    exit 1
fi

if [[ $RC -eq 137 || $RC -eq 124 ]]; then
    echo "NOTE: wall ${WALL_SEC}s hit during Vk pipeline compile — boot path OK, run longer once to warm cache"
fi

echo "OK: 20s boot smoke passed (rc=$RC, markers=$PASS/4)"
exit 0