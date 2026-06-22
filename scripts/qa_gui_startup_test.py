#!/usr/bin/env python3
"""QA: real SDL window + swapchain path (non-headless GUI boot)."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "build" / "bin" / "Linux" / "AMOURANTHRTX"
LOG = ROOT / "build" / "qa_gui_startup.log"
# Cold aos_load Vk compile can exceed 40min; GUI uses same pipeline path.
WALL_SEC = 5400
SMOKE_WALL_SEC = 45


def fail(msg: str) -> int:
    print(f"FAIL {msg}", file=sys.stderr)
    return 1


def ok(msg: str) -> None:
    print(f"OK {msg}")


def window_visible(pid: int) -> bool:
    if not os.environ.get("DISPLAY"):
        return False
    for tool, args in (
        ("xdotool", ["search", "--pid", str(pid)]),
        ("xdotool", ["search", "--name", "AMOURANTHRTX"]),
    ):
        if not subprocess.run(["which", tool], capture_output=True).returncode == 0:
            continue
        proc = subprocess.run(args, capture_output=True, text=True, timeout=5)
        if proc.returncode == 0 and proc.stdout.strip():
            return True
    return False


def main() -> int:
    if not ENGINE.is_file():
        return fail(f"engine missing: {ENGINE} — run ./linux.sh")

    if not os.environ.get("DISPLAY"):
        return fail("DISPLAY unset — GUI test needs X11")

    LOG.parent.mkdir(parents=True, exist_ok=True)
    LOG.write_text("")

    env = os.environ.copy()
    env.pop("AMOURANTHRTX_HEADLESS", None)
    env.pop("HEADLESS", None)
    env.pop("AMOURANTHRTX_MAX_FRAMES", None)
    env["AMOURANTHRTX_NO_VALIDATION"] = "1"
    env["VK_INSTANCE_LAYERS"] = ""

    print(f"=== GUI startup: {SMOKE_WALL_SEC}s window probe, then up to {WALL_SEC}s boot ===")
    print(f"log: {LOG}")

    proc = subprocess.Popen(
        [str(ENGINE)],
        cwd=str(ENGINE.parent),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    t0 = time.monotonic()
    saw_window = False
    saw_swapchain = False
    saw_raycanvas = False
    saw_hybrid = False
    saw_shutdown = False
    out_lines: list[str] = []

    try:
        assert proc.stdout is not None
        while True:
            if proc.poll() is not None:
                break
            elapsed = time.monotonic() - t0
            if elapsed > WALL_SEC:
                proc.kill()
                proc.wait(timeout=10)
                return fail(f"GUI boot timed out after {WALL_SEC}s")

            line = proc.stdout.readline()
            if not line:
                time.sleep(0.05)
                if elapsed > 2.0 and not saw_window and proc.pid:
                    saw_window = window_visible(proc.pid)
                continue

            out_lines.append(line)
            with LOG.open("a", encoding="utf-8", errors="replace") as f:
                f.write(line)

            if "Main window shown" in line:
                saw_window = True
            if re.search(r"Swapchain ready", line):
                saw_swapchain = True
            if re.search(r"CTOR: begin RayCanvas|RayCanvas created", line):
                saw_raycanvas = True
            if "Initialized hybrid renderer" in line:
                saw_hybrid = True
            if re.search(r"Engine shutdown complete|Canvas destroyed", line):
                saw_shutdown = True
                break

            if elapsed <= SMOKE_WALL_SEC and not saw_window and proc.pid:
                saw_window = window_visible(proc.pid)

            if saw_hybrid and elapsed > 5.0:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)
                break
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait(timeout=10)

    out = "".join(out_lines)
    elapsed = time.monotonic() - t0

    if "HEADLESS" in out and "Main window shown" not in out:
        return fail("engine entered headless mode — GUI path not exercised")

    if not saw_window and not re.search(r"Main window shown", out):
        return fail("SDL window never became visible")

    if not saw_swapchain:
        return fail("real swapchain never initialized")

    if not saw_raycanvas:
        return fail(f"RayCanvas ctor did not start within {elapsed:.1f}s")

    if not saw_hybrid:
        return fail(
            f"hybrid renderer not ready within {elapsed:.1f}s "
            "(aos_load Vk cold compile may still be running — warm cache first)"
        )

    ok(f"GUI startup {elapsed:.1f}s — window, swapchain, RayCanvas, hybrid renderer")
    if saw_shutdown:
        ok("clean shutdown observed")
    else:
        print("NOTE: terminated after hybrid renderer ready (bounded GUI smoke)")
    print("GUI startup QA passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())