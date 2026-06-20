#!/usr/bin/env python3
"""NES Contra QA — headless guest VGA snap + audio level check."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GRAB = ROOT / "build" / "grabs" / "nes"
BIN_LINUX = ROOT / "build" / "bin" / "Linux" / "AMOURANTHRTX"


def run(cmd: list[str], cwd: Path | None = None, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(
        cmd, cwd=cwd or ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=timeout, check=False,
    )


def ppm_to_png(ppm: Path) -> Path:
    png = ppm.with_suffix(".png")
    if shutil.which("convert"):
        subprocess.run(["convert", str(ppm), str(png)], check=True)
        return png
    from PIL import Image
    data = ppm.read_bytes()
    header, _, rest = data.partition(b"\n255\n")
    lines = header.decode("ascii", errors="ignore").strip().split("\n")
    w, h = map(int, lines[1].split())
    img = Image.frombytes("RGB", (w, h), rest[: w * h * 3])
    img.save(png)
    return png


def framebuffer_stats(png: Path) -> tuple[int, int, float]:
    from PIL import Image
    from collections import Counter
    im = Image.open(png).convert("RGB")
    px = list(im.getdata())
    w, h = im.size
    unique = len(set(px))
    edge_hits = sum(
        1 for y in range(h - 1) for x in range(w - 1)
        if px[y * w + x] != px[y * w + x + 1] or px[y * w + x] != px[(y + 1) * w + x]
    )
    top = Counter(px).most_common(1)[0][1]
    dominance = top / max(len(px), 1)
    return unique, edge_hits, dominance


def capture_nes_snap(engine: Path, tag: str, cwd: Path) -> tuple[Path, str]:
    GRAB.mkdir(parents=True, exist_ok=True)
    ppm = GRAB / f"{tag}_contra_fb.ppm"
    ppm.unlink(missing_ok=True)
    env = {
        "AMOURANTHRTX_HEADLESS": "1",
        "AMOURANTHRTX_NO_VALIDATION": "1",
        "VK_INSTANCE_LAYERS": "",
        "AMOURANTHRTX_NES_TEST": "1",
        "AMOURANTHRTX_BENCH_W": "1280",
        "AMOURANTHRTX_BENCH_H": "720",
        "AMOURANTHRTX_MAX_FRAMES": "300",
        "AMOURANTHRTX_NES_FB_SNAP": str(ppm),
        "AMOURANTHRTX_NES_FB_FRAME": "240",
    }
    proc = run(["env", *[f"{k}={v}" for k, v in env.items()], str(engine)], cwd=cwd, timeout=180)
    log = (proc.stderr or "") + (proc.stdout or "")
    if not ppm.is_file():
        raise SystemExit(f"FAIL NES snap missing rc={proc.returncode}\n{log[-1200:]}")
    if "[NES_QA] launched" not in log:
        raise SystemExit(f"FAIL Contra never launched\n{log[-1200:]}")
    png = ppm_to_png(ppm)
    print(f"  snap {png}")
    return png, log


def check_audio(log: str) -> float:
    levels = [float(m.group(1)) for m in re.finditer(r"\[NES_QA\].*audioLevel=([0-9.]+)", log)]
    if not levels:
        raise SystemExit("FAIL no NES audioLevel logged")
    peak = max(levels)
    if peak < 0.05:
        raise SystemExit(f"FAIL NES audio too quiet (peak={peak:.4f})")
    print(f"OK NES audio peak={peak:.4f}")
    return peak


def check_framebuffer(png: Path) -> None:
    unique, edge_hits, dominance = framebuffer_stats(png)
    if edge_hits < 4000:
        raise SystemExit(
            f"FAIL [{png.name}] Contra framebuffer too flat "
            f"(edges={edge_hits}, unique={unique}, dominance={dominance:.3f})"
        )
    if unique < 3:
        raise SystemExit(
            f"FAIL [{png.name}] Contra palette too uniform (unique={unique})"
        )
    if dominance > 0.98:
        raise SystemExit(
            f"FAIL [{png.name}] Contra framebuffer single-color wash (dominance={dominance:.3f})"
        )
    print(f"OK [{png.name}] Contra FB (edges={edge_hits}, unique={unique}, dominance={dominance:.3f})")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.parse_args()

    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pillow", "-q"], check=True)

    if not BIN_LINUX.is_file():
        run(["./linux.sh"], timeout=300).check_returncode()
    else:
        run(["cmake", "--build", str(ROOT / "build"), "--target", "amouranth_engine", "copy_assets"],
            timeout=300).check_returncode()

    png, log = capture_nes_snap(BIN_LINUX, "linux", BIN_LINUX.parent)
    check_framebuffer(png)
    check_audio(log)
    print("\nNES Contra visual + audio QA PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())