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
CACHE_PATHS = (
    BUILD / "bin" / "Linux" / "cache" / "vulkan_compute.cache",
    ROOT / "cache" / "vulkan_compute.cache",
)
MAX_FRAMES = 24
WARM_WALL_SEC = 60
WARM_EXIT_SEC = 30


def fail(msg: str) -> int:
    print(f"FAIL {msg}", file=sys.stderr)
    return 1


def ok(msg: str) -> None:
    print(f"OK {msg}")


def pipeline_cache_ready() -> bool:
    return any(p.is_file() and p.stat().st_size > 0 for p in CACHE_PATHS)


def run_smoke() -> int:
    smoke = ROOT / "scripts" / "debug_boot_20s.sh"
    if not smoke.is_file():
        return fail(f"smoke script missing: {smoke}")
    print("Running 20s headless boot smoke …")
    proc = subprocess.run([str(smoke)], cwd=str(ROOT), timeout=35)
    if proc.returncode != 0:
        return fail("debug_boot_20s.sh failed — boot path broken")
    ok("20s headless boot smoke passed")
    return 0


def main() -> int:
    if not ENGINE.is_file():
        print(f"FAIL engine missing: {ENGINE}", file=sys.stderr)
        print("Run: cd AMOURANTHRTX && ./linux.sh", file=sys.stderr)
        return 1

    rc = run_smoke()
    if rc != 0:
        return rc

    warm = pipeline_cache_ready()
    if not warm:
        print(
            "NOTE: no Vulkan pipeline cache — smoke-only pass "
            "(full aos_load Vk compile is 40+ min cold on AMD; run once to populate "
            "build/bin/Linux/cache/vulkan_compute.cache)"
        )
        print("Startup QA passed (smoke)")
        return 0

    wall_sec = WARM_WALL_SEC
    exit_sec = WARM_EXIT_SEC
    print("Vulkan pipeline cache present — full headless QA")

    env = os.environ.copy()
    env["AMOURANTHRTX_HEADLESS"] = "1"
    env["AMOURANTHRTX_MAX_FRAMES"] = str(MAX_FRAMES)
    env["AMOURANTHRTX_NO_VALIDATION"] = "1"
    env["VK_INSTANCE_LAYERS"] = ""

    t0 = time.monotonic()
    proc = subprocess.run(
        [str(ENGINE)],
        cwd=str(ENGINE.parent),
        env=env,
        capture_output=True,
        text=True,
        timeout=wall_sec,
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
        return fail(f"RayCanvas ctor never finished within {wall_sec}s")

    if "MAX_FRAMES" not in out and "Canvas destroyed" not in out and "Engine shutdown complete" not in out:
        return fail("clean headless exit markers missing")

    if elapsed > exit_sec:
        return fail(f"headless run took {elapsed:.1f}s — expected < {exit_sec}s with warm cache")

    ok(f"startup headless {elapsed:.1f}s — aos_load stub, no hotswap, clean exit")
    print("Startup QA passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())