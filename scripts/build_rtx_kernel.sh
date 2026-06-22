#!/usr/bin/env bash
# Build KILROY Field Die kernel (delegates to SG/KILROY build-kilroy.sh).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=kilroy_root.sh
source "$ROOT/scripts/kilroy_root.sh"

OUT="${OUT:-$ROOT/build/kernel-kilroy}"
GROK_IMAGE="${GROK_IMAGE:-0}"

echo "=== AMOURANTHRTX → KILROY Field Die kernel ==="
echo "    KILROY_ROOT: $KILROY_ROOT"
echo "    OUT:         $OUT"

[[ -x "$KILROY_ROOT/scripts/build-kilroy.sh" ]] || {
    echo "[rtx-kernel] missing: $KILROY_ROOT/scripts/build-kilroy.sh"
    exit 1
}

mkdir -p "$OUT"
GROK_IMAGE="$GROK_IMAGE" OUT="$KILROY_ROOT/build" "$KILROY_ROOT/scripts/build-kilroy.sh"
cp -f "$KILROY_ROOT/build/bzImage" "$OUT/bzImage"
[[ -f "$KILROY_ROOT/build/config" ]] && cp -f "$KILROY_ROOT/build/config" "$OUT/config"
echo "[rtx-kernel] bzImage -> $OUT/bzImage"
echo "[rtx-kernel] Telemetry: /proc/kilroy_field/{status,power,fcc,thermo}"