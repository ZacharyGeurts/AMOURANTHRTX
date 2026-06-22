#!/usr/bin/env python3
"""Shareware Doom GPU QA — mode 13h assets + framebuffer compare (no libx86emu)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GRAB = ROOT / "build" / "grabs" / "doom"
BIN = ROOT / "build" / "bin" / "Linux"
GPU_TEST = BIN / "qa_doom_gpu_test"
REF_TITLE = ROOT / "assets" / "doom_gpu" / "title320x200.bin"
REF_PAL = ROOT / "assets" / "doom_gpu" / "palette.bin"


def run(cmd: list[str], *, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(
        cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout, check=False,
    )


def build_gpu_test() -> None:
    BIN.mkdir(parents=True, exist_ok=True)
    candidates = [
        ROOT / "build" / "qa_doom_gpu_test",
        ROOT / "build" / "bin" / "Linux" / "qa_doom_gpu_test",
    ]
    for built in candidates:
        if built.is_file():
            shutil.copy2(built, GPU_TEST)
            return
    subprocess.run(
        [
            "cmake", "--build", str(ROOT / "build"),
            "--target", "qa_doom_gpu_test", "-j", str(os.cpu_count() or 4),
        ],
        cwd=ROOT,
        check=True,
    )
    for built in candidates:
        if built.is_file():
            shutil.copy2(built, GPU_TEST)
            return
    raise SystemExit("FAIL qa_doom_gpu_test binary missing after cmake build")


def ppm_to_rgb(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    try:
        from PIL import Image
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pillow", "-q"], check=True)
        from PIL import Image
    img = Image.open(path).convert("RGB")
    return img.size[0], img.size[1], list(img.getdata())


def load_ref_title() -> list[tuple[int, int, int]]:
    pal = REF_PAL.read_bytes()
    raw = REF_TITLE.read_bytes()
    out: list[tuple[int, int, int]] = []
    for idx in raw:
        off = idx * 3
        out.append((pal[off], pal[off + 1], pal[off + 2]))
    return out


def compare_images(
    got: list[tuple[int, int, int]],
    ref: list[tuple[int, int, int]],
    *,
    min_match: float,
    min_nz: int,
) -> tuple[float, int]:
    if len(got) != len(ref):
        raise SystemExit(f"FAIL size mismatch got={len(got)} ref={len(ref)}")
    nz = sum(1 for p in got if sum(p) > 12)
    hits = sum(
        1
        for a, b in zip(got, ref)
        if abs(a[0] - b[0]) <= 18 and abs(a[1] - b[1]) <= 18 and abs(a[2] - b[2]) <= 18
    )
    ratio = hits / len(ref)
    if nz < min_nz:
        raise SystemExit(f"FAIL framebuffer too empty (nz={nz}, need>={min_nz})")
    if ratio < min_match:
        raise SystemExit(f"FAIL screenshot match {ratio:.3f} < {min_match:.3f}")
    return ratio, nz


def main() -> int:
    ap = argparse.ArgumentParser(description="GPU Doom mode-13h QA")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--min-match", type=float, default=0.55)
    args = ap.parse_args()

    if not REF_TITLE.is_file() or not REF_PAL.is_file():
        print("Missing doom_gpu assets — run: python3 scripts/extract_doom_gpu.py", file=sys.stderr)
        return 1

    if not args.skip_build:
        build_gpu_test()

    GRAB.mkdir(parents=True, exist_ok=True)
    proc = run([str(GPU_TEST), str(GRAB)])
    print(proc.stdout, end="")
    print(proc.stderr, end="", file=sys.stderr)
    if proc.returncode != 0:
        return proc.returncode

    title_ppm = GRAB / "gpu_title.ppm"
    if not title_ppm.is_file():
        print(f"FAIL missing {title_ppm}", file=sys.stderr)
        return 1

    _, _, got = ppm_to_rgb(title_ppm)
    ref = load_ref_title()
    ratio, nz = compare_images(got, ref, min_match=args.min_match, min_nz=8000)
    print(f"PASS title match={ratio:.3f} nz={nz}")
    print("GPU Doom QA complete — mode 13h ready, no DOS4GW/libx86emu.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())