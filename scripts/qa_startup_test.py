#!/usr/bin/env python3
"""QA: desktop boots x86 directly; headless bounded runs use aos_load stub (no hotswap)."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
ENGINE = BUILD / "bin" / "Linux" / "AMOURANTHRTX"
MAX_WALL_SEC = 45
MAX_FRAMES = 24
MAX_EXIT_SEC = 20


def fail(msg: str) -> int:
    print(f"FAIL {msg}", file=sys.stderr)
    return 1


def ok(msg: str) -> None:
    print(f"OK {msg}")


def main() -> int:
    if not ENGINE.is_file():
        print(f"FAIL engine missing: {ENGINE}", file=sys.stderr)
        print("Run: cd AMOURANTHRTX && ./linux.sh", file=sys.stderr)
        return 1

    env = os.environ.copy()
    env["AMOURANTHRTX_HEADLESS"] = "1"
    env["AMOURANTHRTX_MAX_FRAMES"] = str(MAX_FRAMES)
    env["VK_INSTANCE_LAYERS"] = ""

    t0 = time.monotonic()
    proc = subprocess.run(
        [str(ENGINE)],
        cwd=str(ENGINE.parent),
        env=env,
        capture_output=True,
        text=True,
        timeout=MAX_WALL_SEC,
    )
    elapsed = time.monotonic() - t0
    out = proc.stdout + proc.stderr

    if proc.returncode < 0:
        return fail(f"engine killed by signal {-proc.returncode} after {elapsed:.1f}s")

    if "aos_load" not in out and "stub" not in out:
        return fail("aos_load boot path not logged")

    if not re.search(r"Pipeline ready:\s*aos_load", out):
        return fail("headless aos_load stub pipeline never created")

    if re.search(r"Promoted aos_load|Background x86 compile started", out, re.I):
        return fail("background x86 compile should not run in headless bounded mode")

    if "Initialized hybrid renderer" not in out:
        return fail(f"RayCanvas ctor never finished within {MAX_WALL_SEC}s")

    if "MAX_FRAMES" not in out and "Canvas destroyed" not in out and "Engine shutdown complete" not in out:
        return fail("clean headless exit markers missing")

    if elapsed > MAX_EXIT_SEC:
        return fail(f"headless run took {elapsed:.1f}s — expected < {MAX_EXIT_SEC}s")

    ok(f"startup headless {elapsed:.1f}s — aos_load stub, no hotswap, clean exit")
    print("Startup QA passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
