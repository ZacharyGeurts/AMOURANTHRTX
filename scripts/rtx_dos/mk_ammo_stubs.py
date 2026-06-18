#!/usr/bin/env python3
"""RTX-AMMOS field-layer .SYS drivers — AMMOASM build via mk_supercore.py."""

from __future__ import annotations

import sys
from pathlib import Path

_BUILD = Path(__file__).resolve().parent
if str(_BUILD) not in sys.path:
    sys.path.insert(0, str(_BUILD))

# Re-export constants for tooling that imported this module.
from mk_supercore import (  # noqa: F401
    LAYER_AUDIO,
    LAYER_BIOS,
    LAYER_DRIVES,
    LAYER_FAT,
    LAYER_INPUT,
    LAYER_IO,
    LAYER_MSCDEX,
    LAYER_RAM,
    LAYER_VGA,
    LAYER_VIEWPORT,
    LAYR_MAGIC,
    RTXFL_MAGIC,
    build_field_sys,
    emit_all,
)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    out = Path(sys.argv[1] if len(sys.argv) > 1 else root / "assets" / "dos" / "ammo")
    emit_all(out)
    print(f"wrote AMMOASM field-layer drivers via supercore → {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())