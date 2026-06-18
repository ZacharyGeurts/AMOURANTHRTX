#!/usr/bin/env bash
# Run QA + DOS shell grab test + optional live window capture.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin/Linux"
GRAB="$ROOT/build/grabs"
LIB="$ROOT/build/libx86emu.a"
ENGINE="$BIN/AMOURANTHRTX"

mkdir -p "$GRAB"

echo "================================================================"
echo " AMOURANTHRTX — run + test + image grab"
echo "================================================================"
echo

echo "=== 1) Full QA suite (complete output) ==="
"$ROOT/linux.sh" qa 2>&1 | tee "$GRAB/qa_full.log"
echo

echo "=== 2) Drives bench metrics ==="
g++-14 -std=c++20 -O2 -I "$ROOT/Navigator/engine" \
  "$ROOT/scripts/qa_drives_bench_test.cpp" -o "$BIN/qa_drives_bench_test"
"$BIN/qa_drives_bench_test" 2>&1 | tee "$GRAB/bench.log"
echo

echo "=== 3) VGA shell grab (PPM -> PNG) ==="
g++-14 -std=c++20 -O2 -I "$ROOT/Navigator/engine" -I "$ROOT/third_party/libx86emu/include" \
  "$ROOT/scripts/qa_dos_grab_test.cpp" "$LIB" -o "$BIN/qa_dos_grab_test"
"$BIN/qa_dos_grab_test" "$GRAB" 2>&1 | tee "$GRAB/grab.log"

if command -v convert >/dev/null 2>&1; then
  for f in "$GRAB"/*.ppm; do
    [[ -f "$f" ]] || continue
    convert "$f" "${f%.ppm}.png"
    echo "PNG ${f%.ppm}.png"
  done
fi
echo

echo "=== 4) Live engine window grab (5s) ==="
if [[ -x "$ENGINE" && -n "${DISPLAY:-}" ]]; then
  rm -f "$GRAB/engine_window.png"
  (
    cd "$ROOT/build/bin/Linux"
    env AMOURANTHRTX_MAX_FRAMES=480 ./AMOURANTHRTX 2>&1 | tee "$GRAB/engine_run.log"
  ) &
  EPID=$!
  sleep 5
  if command -v xdotool >/dev/null 2>&1; then
    WID=$(xdotool search --pid "$EPID" 2>/dev/null | head -1 || true)
    if [[ -n "$WID" ]]; then
      import -window "$WID" "$GRAB/engine_window.png" 2>/dev/null || true
    fi
  fi
  if [[ ! -f "$GRAB/engine_window.png" ]]; then
    import -window root "$GRAB/desktop_full.png" 2>/dev/null || true
    echo "Wrote desktop_full.png (window id not found)"
  else
    echo "Wrote engine_window.png"
  fi
  kill "$EPID" 2>/dev/null || true
  wait "$EPID" 2>/dev/null || true
else
  echo "SKIP live grab (no DISPLAY or engine missing)"
fi

echo
echo "=== Artifacts ==="
ls -la "$GRAB"
echo
echo "DONE — grabs in $GRAB"