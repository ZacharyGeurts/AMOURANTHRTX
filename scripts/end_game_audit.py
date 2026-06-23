#!/usr/bin/env python3
"""END OF EVERYTHING MODE — full QA matrix (fast path, skips doom)."""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def run_py(script: str, *extra: str) -> int:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / script), *extra],
        cwd=ROOT,
        check=False,
    )
    return proc.returncode


def ensure_build() -> int:
    if (BUILD / "qa_fieldstorage_test").is_file():
        return 0
    proc = subprocess.run(
        ["cmake", "--build", str(BUILD), "-j", "8"],
        cwd=ROOT,
        check=False,
    )
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--with-doom", action="store_true", help="Include doom QA (slow)")
    args = parser.parse_args()

    os.environ["AMOURANTHRTX_END_GAME"] = "1"
    os.environ.setdefault("AMOURANTHRTX_INFINITE", "1")
    os.environ.setdefault("AMOURANTHRTX_ALL_BREAKTHROUGHS", "1")
    os.environ.setdefault("AMOURANTHRTX_EVERYTHING_EVERYWHERE", "1")
    os.environ.setdefault("AMOURANTHRTX_FIELD_PERSIST", "1")

    if ensure_build() != 0:
        print("FAIL end_game build", file=sys.stderr)
        return 1

    if run_py("monolith_audit.py") != 0:
        return 1

    audit_args = ["--end-game"]
    if args.with_doom:
        audit_args.append("--with-doom")
    if run_py("zero_cost_audit.py", *audit_args) != 0:
        return 1

    for script in ("play_legacy.py", "team_drive_test.py", "bench_storage.py", "bench_mame_compare.py"):
        if run_py(script) != 0:
            print(f"FAIL end_game {script}", file=sys.stderr)
            return 1

    print("METRIC end_game_mode=1")
    print("OK end_game_audit GREEN ALL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())