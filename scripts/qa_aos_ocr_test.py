#!/usr/bin/env python3
"""AmouranthOS Start button OCR — GPU snapshot verifies label + menu popout."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GRAB = ROOT / "build" / "grabs" / "aos"
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


def ocr_region(png: Path, box: tuple[int, int, int, int]) -> str:
    from PIL import Image
    img = Image.open(png).crop(box)
    tmp = png.parent / f"_ocr_{png.stem}.png"
    img.save(tmp)
    if not shutil.which("tesseract"):
        return ""
    proc = subprocess.run(
        ["tesseract", str(tmp), "stdout", "--psm", "7", "-c", "tessedit_char_whitelist=STARTPROGRAMRTX"],
        capture_output=True, text=True, check=False,
    )
    tmp.unlink(missing_ok=True)
    return proc.stdout.upper().replace("\n", " ").strip()


def rose_score(rgb: tuple[int, int, int]) -> float:
    r, g, b = rgb
    if r < 45:
        return 0.0
    magenta = (r > 80 and b > 60 and r > g + 10)
    rose = (r - g) / 255.0 + (r - b) / 255.0
    return max(rose, 0.35 if magenta else 0.0)


def sample_region(png: Path, box: tuple[int, int, int, int]) -> list[tuple[int, int, int]]:
    from PIL import Image
    return list(Image.open(png).crop(box).getdata())


def check_start_button(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    box = (0, h - task_h - 4, int(w * 0.22), h)
    pixels = sample_region(png, box)
    rose_hits = sum(1 for p in pixels if rose_score(p) > 0.08)
    text_box = (int(w * 0.04), h - task_h + 6, int(w * 0.20), h - 6)
    label_px = sample_region(png, text_box)
    label_bright = sum(1 for p in label_px if sum(p) > 200)
    text = ocr_region(png, text_box)
    if rose_hits < 80 and label_bright < 4:
        raise SystemExit(
            f"FAIL [{png.name}] Start button missing (rose={rose_hits}, bright={label_bright})"
        )
    if text and not any(k in text for k in ("STA", "START", "TAR", "RTX")):
        raise SystemExit(f"FAIL [{png.name}] Start label OCR: {text!r}")
    if not text and label_bright < 4:
        raise SystemExit(f"FAIL [{png.name}] Start label not visible (bright={label_bright})")
    print(f"OK [{png.name}] Start button (rose={rose_hits}, bright={label_bright}, ocr={text!r})")


def check_start_menu(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    menu_h = int(h * 0.38)
    box = (0, h - task_h - menu_h, int(w * 0.28), h - task_h)
    pixels = sample_region(png, box)
    dark = sum(1 for p in pixels if sum(p) < 95)
    bright = sum(1 for p in pixels if sum(p) > 180)
    if dark < len(pixels) * 0.20:
        raise SystemExit(f"FAIL [{png.name}] Start menu popout not visible")
    text = ocr_region(png, (8, h - task_h - menu_h + 24, int(w * 0.24), h - task_h - 12))
    if text and any(k in text for k in ("RTX", "PROG", "PRO", "SHELL", "TOOL", "GAME")):
        print(f"OK [{png.name}] Start menu popout (ocr={text!r})")
        return
    if bright < 80:
        raise SystemExit(
            f"FAIL [{png.name}] Start menu popout text not visible (ocr={text!r}, bright={bright})"
        )
    print(f"OK [{png.name}] Start menu popout (bright={bright}, ocr={text!r})")


def capture_start_snap(engine: Path, tag: str, cwd: Path) -> Path:
    GRAB.mkdir(parents=True, exist_ok=True)
    ppm = GRAB / f"{tag}_start_menu.ppm"
    ppm.unlink(missing_ok=True)
    env = {
        "AMOURANTHRTX_HEADLESS": "1",
        "AMOURANTHRTX_AOS_TEST": "1",
        "AMOURANTHRTX_BENCH_W": "1280",
        "AMOURANTHRTX_BENCH_H": "720",
        "AMOURANTHRTX_MAX_FRAMES": "35",
        "AMOURANTHRTX_SNAP_OUT": str(ppm),
        "AMOURANTHRTX_SNAP_FRAME": "30",
    }
    proc = run(["env", *[f"{k}={v}" for k, v in env.items()], str(engine)], cwd=cwd, timeout=180)
    if not ppm.is_file():
        tail = (proc.stderr or proc.stdout or "")[-800:]
        raise SystemExit(f"FAIL snap rc={proc.returncode}\n{tail}")
    png = ppm_to_png(ppm)
    print(f"  snap {png}")
    return png


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
            timeout=300)

    png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent)
    from PIL import Image
    with Image.open(png) as im:
        w, h = im.size
    check_start_button(png, w, h)
    check_start_menu(png, w, h)
    print("\nAmouranthOS Start button OCR QA PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())