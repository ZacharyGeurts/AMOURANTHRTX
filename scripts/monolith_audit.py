#!/usr/bin/env python3
"""Monolith audit — verifies core Field/CHIPS/Keen/end-game symbols are wired."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MONOLITHS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("Navigator/engine/FieldStorage.hpp", (
        "enableEndGameMode", "enableAllBreakthroughs", "sdfFoldBlock", "dualHostReady",
        "persistFieldState", "restoreFieldState",
    )),
    ("Navigator/engine/FieldStorage.cpp", (
        "phiSuperposition", "mountMultiFS", "hyperTick", "persistFieldState", "restoreFieldState",
    )),
    ("Navigator/engine/FieldEverything.hpp", (
        "Everything Everywhere", "seedChips", "persistFieldState", "no load",
    )),
    ("Navigator/engine/FieldFabric.hpp", (
        "dispatchExtended", "entropyFabricPredict", "processLeadIn", "gEntropyFold",
    )),
    ("Navigator/engine/Pipeline.hpp", (
        "bootstrapEndGameOnce", "ENABLE_ALL_BREAKTHROUGHS", "FieldAmiga::tick",
        "FieldHostess7::tick",
    )),
    ("Navigator/engine/FieldHostess7.hpp", (
        "BUS_HOSTESS_LIVE", "hostess_native", "packDataBus",
    )),
    ("docs/HOSTESS7_V33.md", (
        "Hostess 7", "Turn Over", "presumption",
    )),
    ("scripts/field_superintelligence.py", (
        "turnover", "protocol_v33", "TURNOVER_QUESTIONS",
    )),
    ("dos/FieldRtxPm.hpp", ("keenLaunchProgress", "ipProgressProbe")),
    ("dos/FieldRtxLe.hpp", ("forceTitleBlit", "keenTitleBlitProbe", "keenTitleStalled")),
    ("Navigator/engine/FieldGpuLaunch.hpp", ("seedKeenMzExec", "keenTitleBlitSeeded", "forceTitleBlit")),
    ("AmmoOS/core/FieldAosChipsWave.hpp", ("openAmigaLove", "openLoveOfEverything")),
    ("Navigator/engine/CHIPS/Common/FieldChipFabricScale.hpp", ("scaledDieCycles",)),
    ("linux.sh", ("--end-game", "end-game", "AMOURANTHRTX_END_GAME", "everything-everywhere", "AMOURANTHRTX_FIELD_PERSIST", "turnover")),
    ("scripts/end_game_audit.py", ("zero_cost_audit", "end_game_mode")),
    ("scripts/zero_cost_audit.py", ("keen_ip_progress", "hyper_breakthroughs")),
)


def audit() -> bool:
    ok = True
    for rel, needles in MONOLITHS:
        path = ROOT / rel
        if not path.is_file():
            print(f"FAIL monolith missing {rel}", file=sys.stderr)
            ok = False
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for needle in needles:
            if needle not in text:
                print(f"FAIL monolith {rel} missing '{needle}'", file=sys.stderr)
                ok = False
            else:
                print(f"METRIC monolith_{rel.replace('/', '_')}_{needle}=1")
    if ok:
        print("METRIC monolith_audit=1")
        print("OK monolith_audit all symbols present")
    return ok


def main() -> int:
    return 0 if audit() else 1


if __name__ == "__main__":
    raise SystemExit(main())