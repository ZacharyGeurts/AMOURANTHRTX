#!/usr/bin/env python3
"""OCR + ink-scan toolkit — locate taskbar/chrome click targets from GPU grabs.

Mirrors FieldTaskbarLayout / FieldAmouranthOs uiScale at 3840 reference.
Writes build/ocr_click_cal.json for QA and headless click drivers.
"""

from __future__ import annotations

import json
import math
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
GRAB = ROOT / "build" / "grabs" / "ocr_click"
CAL_OUT = ROOT / "build" / "ocr_click_cal.json"
OCR_REVIEW = ROOT.parent / "ocr"

AOS_UI_REF_W = 3840.0
TASKBAR_H = 56.0
START_W = 156.0
QUICK_W = 44.0
QUICK_N = 4
UI_BOOST = 1.35
MAX_DRIFT_PX = 18.0


@dataclass
class ClickTarget:
    name: str
    layout_x: float
    layout_y: float
    ocr_x: float | None = None
    ocr_y: float | None = None
    drift: float | None = None
    box: tuple[int, int, int, int] | None = None


def ui_scale(vp_w: float) -> float:
    return max(vp_w / AOS_UI_REF_W, 0.75) * UI_BOOST


def taskbar_layout(vp_w: float, vp_h: float) -> dict[str, float]:
    sc = ui_scale(vp_w)
    task_h = TASKBAR_H * sc
    pad = 6.0 * sc
    lift = 22.0 * sc
    bar_y0 = vp_h - task_h
    bar_y1 = vp_h
    start_x = pad
    start_w = START_W * sc
    btn_y0 = bar_y0 + pad - lift
    btn_y1 = bar_y1 - pad
    quick_x = start_x + start_w + 4.0 * sc
    quick_w = QUICK_W * sc
    tab_x = quick_x + QUICK_N * (quick_w + 4.0 * sc) + 6.0 * sc
    tab_w = 40.0 * sc
    clock_w = 200.0 * sc
    clock_x0 = vp_w - pad - clock_w
    return {
        "scale": sc,
        "task_h": task_h,
        "bar_y0": bar_y0,
        "bar_y1": bar_y1,
        "btn_y0": btn_y0,
        "btn_y1": btn_y1,
        "start_x": start_x,
        "start_w": start_w,
        "quick_x": quick_x,
        "quick_w": quick_w,
        "tab_x": tab_x,
        "tab_w": tab_w,
        "clock_x0": clock_x0,
        "clock_w": clock_w,
        "pad": pad,
    }


def layout_targets(vp_w: float, vp_h: float) -> list[ClickTarget]:
    L = taskbar_layout(vp_w, vp_h)
    cy = (L["btn_y0"] + L["btn_y1"]) * 0.5
    out = [
        ClickTarget("start", L["start_x"] + L["start_w"] * 0.5, cy),
    ]
    for qi in range(QUICK_N):
        qx = L["quick_x"] + qi * (L["quick_w"] + 4.0 * L["scale"])
        out.append(ClickTarget(f"quick_{qi}", qx + L["quick_w"] * 0.5, cy))
    out.append(ClickTarget("tab_0", L["tab_x"] + L["tab_w"] * 0.5, L["bar_y0"] + 5.0 * L["scale"] + (L["task_h"] - 10.0 * L["scale"]) * 0.5))
    out.append(ClickTarget(
        "clock",
        L["clock_x0"] + L["clock_w"] * 0.5,
        L["bar_y0"] + L["task_h"] * 0.5,
    ))
    return out


def ink_score(rgb: tuple[int, int, int]) -> float:
    r, g, b = rgb
    if r > 200 and g > 200 and b > 210:
        return max(0.0, (r - 200) / 55.0 + (g - 200) / 55.0 + (b - 210) / 45.0)
    if r < 95 and g < 95 and b < 105:
        return max(0.0, (90 - r) / 90.0 + (90 - g) / 90.0 + (90 - b) / 90.0)
    return 0.0


def sample_region(png: Path, box: tuple[int, int, int, int]) -> list[tuple[int, int, int]]:
    from PIL import Image
    return list(Image.open(png).crop(box).getdata())


def ocr_region(
    png: Path,
    box: tuple[int, int, int, int],
    *,
    whitelist: str = "STARTPROGRAMRTX0123456789:",
    upscale: int = 2,
) -> str:
    from PIL import Image
    img = Image.open(png).crop(box)
    if upscale > 1:
        img = img.resize((img.width * upscale, img.height * upscale), Image.Resampling.NEAREST)
    tmp = png.parent / f"_ocr_{png.stem}_{box[0]}_{box[1]}.png"
    img.save(tmp)
    if not shutil.which("tesseract"):
        tmp.unlink(missing_ok=True)
        return ""
    proc = subprocess.run(
        ["tesseract", str(tmp), "stdout", "--psm", "7",
         "-c", f"tessedit_char_whitelist={whitelist}"],
        capture_output=True, text=True, check=False,
    )
    tmp.unlink(missing_ok=True)
    return proc.stdout.upper().replace("\n", " ").strip()


def find_ink_centroid(png: Path, box: tuple[int, int, int, int], *, min_score: float = 0.12) -> tuple[float, float] | None:
    from PIL import Image
    img = Image.open(png).crop(box)
    w, h = img.size
    if w < 2 or h < 2:
        return None
    xs, ys, wt = 0.0, 0.0, 0.0
    for y in range(h):
        for x in range(w):
            s = ink_score(img.getpixel((x, y)))
            if s >= min_score:
                xs += x * s
                ys += y * s
                wt += s
    if wt < 4.0:
        return None
    ox, oy, _, _ = box
    return ox + xs / wt, oy + ys / wt


def detect_start_region(png: Path, w: int, h: int) -> tuple[tuple[int, int, int, int], tuple[float, float] | None]:
    L = taskbar_layout(float(w), float(h))
    pad = int(max(4, L["pad"]))
    box = (
        int(L["start_x"]) - pad,
        int(L["btn_y0"]) - pad,
        int(L["start_x"] + L["start_w"]) + pad,
        int(L["btn_y1"]) + pad,
    )
    return box, find_ink_centroid(png, box)


def detect_quick_regions(png: Path, w: int, h: int) -> list[tuple[str, tuple[int, int, int, int], tuple[float, float] | None]]:
    L = taskbar_layout(float(w), float(h))
    pad = int(max(3, L["pad"] * 0.5))
    found: list[tuple[str, tuple[int, int, int, int], tuple[float, float] | None]] = []
    for qi in range(QUICK_N):
        qx = L["quick_x"] + qi * (L["quick_w"] + 4.0 * L["scale"])
        box = (int(qx), int(L["btn_y0"]), int(qx + L["quick_w"]), int(L["btn_y1"]))
        box = (box[0] - pad, box[1] - pad, box[2] + pad, box[3] + pad)
        found.append((f"quick_{qi}", box, find_ink_centroid(png, box, min_score=0.08)))
    return found


def detect_clock_region(png: Path, w: int, h: int) -> tuple[tuple[int, int, int, int], tuple[float, float] | None, str]:
    L = taskbar_layout(float(w), float(h))
    box = (
        int(L["clock_x0"]),
        int(L["bar_y0"] + 4),
        int(L["clock_x0"] + L["clock_w"]),
        int(L["bar_y1"] - 4),
    )
    text = ocr_region(png, box, whitelist="0123456789:", upscale=3)
    centroid = find_ink_centroid(png, box, min_score=0.10)
    return box, centroid, text


def save_review_crop(png: Path, box: tuple[int, int, int, int], name: str) -> Path:
    from PIL import Image
    OCR_REVIEW.mkdir(parents=True, exist_ok=True)
    img = Image.open(png).crop(box)
    img = img.resize((img.width * 2, img.height * 2), Image.Resampling.NEAREST)
    out = OCR_REVIEW / name
    img.save(out)
    return out


def calibrate_from_png(png: Path, *, vp_w: int | None = None, vp_h: int | None = None) -> dict[str, Any]:
    from PIL import Image
    with Image.open(png) as im:
        w, h = im.size
    if vp_w:
        w = vp_w
    if vp_h:
        h = vp_h

    targets = {t.name: t for t in layout_targets(float(w), float(h))}

    start_box, start_c = detect_start_region(png, w, h)
    if start_c:
        t = targets["start"]
        t.ocr_x, t.ocr_y = start_c
        t.box = start_box
        t.drift = math.hypot(t.ocr_x - t.layout_x, t.ocr_y - t.layout_y)

    for name, box, centroid in detect_quick_regions(png, w, h):
        if name not in targets or centroid is None:
            continue
        t = targets[name]
        t.ocr_x, t.ocr_y = centroid
        t.box = box
        t.drift = math.hypot(t.ocr_x - t.layout_x, t.ocr_y - t.layout_y)

    clock_box, clock_c, clock_text = detect_clock_region(png, w, h)
    if clock_c:
        t = targets["clock"]
        t.ocr_x, t.ocr_y = clock_c
        t.box = clock_box
        t.drift = math.hypot(t.ocr_x - t.layout_x, t.ocr_y - t.layout_y)

    save_review_crop(png, start_box, "ocr_click_start_4x.png")
    save_review_crop(png, clock_box, "ocr_click_clock_4x.png")

    start_text = ocr_region(png, (
        int(targets["start"].layout_x - targets["start"].layout_x * 0.15),
        int(taskbar_layout(w, h)["btn_y0"]),
        int(targets["start"].layout_x + 80),
        int(taskbar_layout(w, h)["btn_y1"]),
    ), whitelist="START ", upscale=3)

    report = {
        "png": str(png),
        "viewport": [w, h],
        "ui_scale": ui_scale(float(w)),
        "clock_ocr": clock_text,
        "start_ocr": start_text,
        "targets": [asdict(t) for t in targets.values()],
        "max_drift_px": MAX_DRIFT_PX,
    }
    return report


def validate_report(report: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    for t in report["targets"]:
        if t["name"] == "tab_0" and t["ocr_x"] is None:
            continue
        if t["ocr_x"] is None:
            if t["name"] in ("start", "quick_0", "clock"):
                errors.append(f"FAIL no ink centroid for {t['name']}")
            continue
        drift = t.get("drift")
        if drift is not None and drift > report["max_drift_px"]:
            errors.append(
                f"FAIL {t['name']} drift {drift:.1f}px "
                f"(layout {t['layout_x']:.1f},{t['layout_y']:.1f} "
                f"ocr {t['ocr_x']:.1f},{t['ocr_y']:.1f})"
            )
    if report.get("start_ocr") and not any(k in report["start_ocr"] for k in ("STA", "START", "RTX")):
        if len(report["start_ocr"]) > 2:
            errors.append(f"FAIL Start OCR unreadable: {report['start_ocr']!r}")
    if report.get("clock_ocr") and ":" not in report["clock_ocr"] and not any(c.isdigit() for c in report["clock_ocr"]):
        errors.append(f"FAIL clock OCR unreadable: {report['clock_ocr']!r}")
    return errors


def write_calibration(report: dict[str, Any]) -> Path:
    CAL_OUT.parent.mkdir(parents=True, exist_ok=True)
    CAL_OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"METRIC ocr_click_cal={CAL_OUT}")
    return CAL_OUT


def capture_snap(engine: Path, tag: str, *, w: int = 3840, h: int = 2160) -> Path:
    GRAB.mkdir(parents=True, exist_ok=True)
    ppm = GRAB / f"{tag}_{w}x{h}.ppm"
    ppm.unlink(missing_ok=True)
    env = {
        "AMOURANTHRTX_HEADLESS": "1",
        "AMOURANTHRTX_NO_VALIDATION": "1",
        "VK_INSTANCE_LAYERS": "",
        "AMOURANTHRTX_AOS_TEST": "1",
        "AMOURANTHRTX_BENCH_W": str(w),
        "AMOURANTHRTX_BENCH_H": str(h),
        "AMOURANTHRTX_MAX_FRAMES": "48",
        "AMOURANTHRTX_SNAP_OUT": str(ppm),
        "AMOURANTHRTX_SNAP_FRAME": "42",
        "AMOURANTHRTX_AOS_SHELL_TEST": "1",
    }
    cmd = ["env", *[f"{k}={v}" for k, v in env.items()], str(engine)]
    print(f"  $ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=engine.parent, capture_output=True, text=True, timeout=240, check=False)
    if not ppm.is_file():
        tail = (proc.stderr or proc.stdout or "")[-1200:]
        raise SystemExit(f"FAIL snap rc={proc.returncode}\n{tail}")
    from PIL import Image
    png = ppm.with_suffix(".png")
    data = ppm.read_bytes()
    header, _, rest = data.partition(b"\n255\n")
    lines = header.decode("ascii", errors="ignore").strip().split("\n")
    iw, ih = map(int, lines[1].split())
    img = Image.frombytes("RGB", (iw, ih), rest[: iw * ih * 3])
    img.save(png)
    print(f"  snap {png} ({iw}x{ih})")
    return png


def ensure_pillow() -> None:
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pillow", "-q"], check=True)


def main() -> int:
    import argparse
    parser = argparse.ArgumentParser(description="OCR click calibration toolkit")
    parser.add_argument("--png", type=Path, help="Existing grab PNG (skip capture)")
    parser.add_argument("--width", type=int, default=3840)
    parser.add_argument("--height", type=int, default=2160)
    parser.add_argument("--write-cal", action="store_true", default=True)
    args = parser.parse_args()

    ensure_pillow()
    if args.png and args.png.is_file():
        png = args.png
    else:
        engine = ROOT / "build" / "bin" / "Linux" / "AMOURANTHRTX"
        if not engine.is_file():
            subprocess.run(["./linux.sh"], cwd=ROOT, check=True, timeout=600)
        else:
            subprocess.run(
                ["cmake", "--build", str(ROOT / "build"), "--target", "amouranth_engine", "copy_assets", "-j8"],
                cwd=ROOT, check=True, timeout=300,
            )
        png = capture_snap(engine, "toolkit", w=args.width, h=args.height)

    report = calibrate_from_png(png, vp_w=args.width, vp_h=args.height)
    if args.write_cal:
        write_calibration(report)

    for t in report["targets"]:
        if t["ocr_x"] is not None:
            print(
                f"OK {t['name']} layout=({t['layout_x']:.1f},{t['layout_y']:.1f}) "
                f"ocr=({t['ocr_x']:.1f},{t['ocr_y']:.1f}) drift={t.get('drift', 0):.1f}px"
            )
        else:
            print(f"SKIP {t['name']} — no OCR centroid")

    errors = validate_report(report)
    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1
    print("OK ocr_click_toolkit GREEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())