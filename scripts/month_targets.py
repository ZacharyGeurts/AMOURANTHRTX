#!/usr/bin/env python3
"""AMOURANTHRTX monthly target matrix — Hostess 7 leadership validation gate."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"

MONTHS: tuple[tuple[str, str, tuple[tuple[str, tuple[str, ...]], ...]], ...] = (
    (
        "1",
        "Core fixes + GUI + Field Drive persistent + SI prototype",
        (
            ("release_checklist_2_0.py", ()),
            ("bench_storage.py", ()),
            ("qa_ocr_click_test.py", ()),
            ("field_superintelligence.py", ("evaluate",)),
        ),
    ),
    (
        "2",
        "CHIPS + sudo tools + everything everywhere + physics grounding",
        (
            ("bench_chips.py", ()),
            ("end_game_audit.py", ()),
            ("bench_mame_compare.py", ()),
            ("field_superintelligence.py", ("physics",)),
        ),
    ),
    (
        "3",
        "Polish + QA + release + offline SI demo",
        (
            ("release_checklist_2_0.py", ()),
            ("qa_aos_ocr_test.py", ()),
            ("field_superintelligence.py", ("lead",)),
            ("play_legacy.py", ()),
        ),
    ),
)

ENV_FLAGS = {
    "AMOURANTHRTX_END_GAME": "1",
    "AMOURANTHRTX_EVERYTHING_EVERYWHERE": "1",
    "AMOURANTHRTX_FIELD_PERSIST": "1",
    "AMOURANTHRTX_INFINITE": "1",
    "AMOURANTHRTX_ALL_BREAKTHROUGHS": "1",
}


def run_step(script: str, *args: str) -> int:
    path = ROOT / "scripts" / script
    if not path.is_file():
        print(f"FAIL month_targets missing {script}", file=sys.stderr)
        return 1
    env = {**os.environ, **ENV_FLAGS}
    proc = subprocess.run(
        [sys.executable, str(path), *args],
        cwd=ROOT,
        env=env,
        check=False,
    )
    if proc.returncode != 0:
        print(f"FAIL month_targets {script} {' '.join(args)}", file=sys.stderr)
    else:
        print(f"OK month_targets {script} {' '.join(args)}".rstrip())
    return proc.returncode


def run_month(month: str) -> int:
    spec = next((m for m in MONTHS if m[0] == month), None)
    if not spec:
        print(f"FAIL month_targets unknown month={month}", file=sys.stderr)
        return 1
    _, label, steps = spec
    print(f"METRIC month={month}")
    print(f"METRIC month_label={label}")
    rc = 0
    for script, args in steps:
        if run_step(script, *args) != 0:
            rc = 1
    if rc == 0:
        print(f"METRIC month_{month}_green=1")
        print(f"OK month_targets month={month} GREEN")
    return rc


def run_all() -> int:
    rc = 0
    for month, label, _ in MONTHS:
        print(f"=== Month {month}: {label} ===")
        if run_month(month) != 0:
            rc = 1
    if rc == 0:
        print("METRIC month_targets_all=1")
        print("OK month_targets ALL MONTHS GREEN")
    return rc


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] in ("all", "--all"):
        return run_all()
    if sys.argv[1].isdigit() and 1 <= int(sys.argv[1]) <= 3:
        return run_month(sys.argv[1])
    print("usage: month_targets.py [1|2|3|all]", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())