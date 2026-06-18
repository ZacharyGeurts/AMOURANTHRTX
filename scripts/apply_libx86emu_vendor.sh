#!/bin/bash
# Re-apply AMOURANTHRTX libx86emu extensions after FetchContent refresh.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DST="${ROOT}/build/_deps/libx86emu-src"
SRC="${ROOT}/scripts/vendor-libx86emu"
if [[ ! -d "$DST" ]]; then
  echo "libx86emu source missing — run cmake configure first" >&2
  exit 1
fi
cp -f "$SRC/fpu.c" "$DST/fpu.c"
cp -f "$SRC/fpu.h" "$DST/include/fpu.h"
cp -f "$SRC/ops2.c" "$DST/ops2.c"
if [[ -f "$SRC/decode.h" ]]; then
  cp -f "$SRC/decode.h" "$DST/include/decode.h"
fi
echo "Applied vendor libx86emu FPU/MMX/LAR overlay to $DST"