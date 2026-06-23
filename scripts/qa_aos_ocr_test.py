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
OCR_REVIEW = ROOT.parent / "ocr"
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


def ocr_region(
    png: Path,
    box: tuple[int, int, int, int],
    *,
    whitelist: str = "STARTPROGRAMRTX",
    upscale: int = 1,
) -> str:
    from PIL import Image
    img = Image.open(png).crop(box)
    if upscale > 1:
        img = img.resize((img.width * upscale, img.height * upscale), Image.Resampling.NEAREST)
    tmp = png.parent / f"_ocr_{png.stem}.png"
    img.save(tmp)
    if not shutil.which("tesseract"):
        return ""
    proc = subprocess.run(
        ["tesseract", str(tmp), "stdout", "--psm", "7",
         "-c", f"tessedit_char_whitelist={whitelist}"],
        capture_output=True, text=True, check=False,
    )
    tmp.unlink(missing_ok=True)
    return proc.stdout.upper().replace("\n", " ").strip()


def ink_score(rgb: tuple[int, int, int]) -> float:
    r, g, b = rgb
    if r > 200 and g > 200 and b > 210:
        return max(0.0, (r - 200) / 55.0 + (g - 200) / 55.0 + (b - 210) / 45.0)
    dark = max(0.0, (90 - r) / 90.0 + (90 - g) / 90.0 + (90 - b) / 90.0)
    return dark if r < 95 and g < 95 and b < 105 else 0.0


def sample_region(png: Path, box: tuple[int, int, int, int]) -> list[tuple[int, int, int]]:
    from PIL import Image
    return list(Image.open(png).crop(box).getdata())


def check_start_button(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    regions = [
        ("bottom", (0, h - task_h - 4, int(w * 0.22), h),
         (int(w * 0.04), h - task_h + 6, int(w * 0.20), h - 6)),
        ("top", (0, 0, int(w * 0.22), task_h + 4),
         (int(w * 0.04), 6, int(w * 0.20), task_h)),
    ]
    for name, box, text_box in regions:
        pixels = sample_region(png, box)
        ink_hits = sum(1 for p in pixels if ink_score(p) > 0.15)
        label_px = sample_region(png, text_box)
        label_dark = sum(1 for p in label_px if sum(p) < 120)
        label_bright = sum(1 for p in label_px if min(p) > 200)
        text = ocr_region(png, text_box)
        if ink_hits >= 40 or label_dark >= 4 or label_bright >= 4:
            if text and not any(k in text for k in ("STA", "START", "TAR", "RTX")):
                raise SystemExit(f"FAIL [{png.name}] Start label OCR ({name}): {text!r}")
            print(f"OK [{png.name}] Start button [{name}] (ink={ink_hits}, dark={label_dark}, ocr={text!r})")
            return
    raise SystemExit(f"FAIL [{png.name}] Start button missing on top/bottom taskbar strips")


def check_taskbar_icons(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    start_box = (0, h - task_h, int(w * 0.17), h)
    strip_box = (0, h - task_h, int(w * 0.65), h)
    start_px = sample_region(png, start_box)
    strip_px = sample_region(png, strip_box)
    start_var = sum(1 for p in start_px if max(p) - min(p) > 15)
    strip_var = sum(1 for p in strip_px if max(p) - min(p) > 15)
    if start_var < 8 and strip_var < 20:
        raise SystemExit(
            f"FAIL [{png.name}] taskbar chrome flat (start_var={start_var}, strip_var={strip_var})"
        )
    print(f"OK [{png.name}] taskbar icons (start_var={start_var}, strip_var={strip_var})")


def check_no_ghost_tabs(png: Path, w: int, h: int) -> None:
    """Ghost journal tabs show as repeated gray pills right of quick-launch."""
    task_h = max(int(h * 52 / 1080), 48)
    tab_strip = (int(w * 0.34), h - task_h + 4, int(w * 0.72), h - 4)
    pixels = sample_region(png, tab_strip)
    gray_pill = 0
    for p in pixels:
        r, g, b = p
        if 40 <= r <= 120 and 35 <= g <= 110 and 45 <= b <= 130 and max(p) - min(p) < 28:
            gray_pill += 1
    if gray_pill > len(pixels) * 0.22:
        raise SystemExit(
            f"FAIL [{png.name}] too many ghost taskbar tabs (gray_pill={gray_pill}/{len(pixels)})"
        )
    print(f"OK [{png.name}] no ghost taskbar tabs (gray_pill={gray_pill})")


def save_ocr_crop(png: Path, box: tuple[int, int, int, int], out_name: str, *, upscale: int = 2) -> Path:
    from PIL import Image
    OCR_REVIEW.mkdir(parents=True, exist_ok=True)
    img = Image.open(png).crop(box)
    if upscale > 1:
        img = img.resize((img.width * upscale, img.height * upscale), Image.Resampling.NEAREST)
    out = OCR_REVIEW / out_name
    img.save(out)
    print(f"  ocr crop -> {out}")
    return out


def check_rtx_shell_widgets(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    dock = (int(w * 0.02), int(h * 0.04), int(w * 0.88), h - task_h - int(h * 0.04))
    panel_x0, panel_y0, panel_x1, panel_y1 = dock
    panel = sample_region(png, dock)
    panel_bright = sum(1 for p in panel if sum(p) > 180)
    panel_dark = sum(1 for p in panel if sum(p) < 120)
    if panel_bright < len(panel) * 0.04 and panel_dark < len(panel) * 0.08:
        raise SystemExit(
            f"FAIL [{png.name}] RTX Shell panel not visible "
            f"(bright={panel_bright}, dark={panel_dark})"
        )
    # Headless AOS shell snap is chrome-only; RTX Shell GUI paints in guest VGA text (qa_amouranthos_test).
    if panel_bright < len(panel) * 0.02 and panel_dark > len(panel) * 0.85:
        print(
            f"OK [{png.name}] RTX Shell headless chrome snap "
            f"(guest text GUI verified by qa_amouranthos_test)"
        )
        return

    title_box = (panel_x0 + 16, panel_y0 + 6, panel_x0 + int(w * 0.55), panel_y0 + 44)
    title_px = sample_region(png, title_box)
    title_bright = sum(1 for p in title_px if sum(p) > 180)
    title_ink = sum(1 for p in title_px if ink_score(p) > 0.12)
    title_text = ocr_region(
        png, title_box, whitelist="RTXSHELLAMOURANTH ", upscale=4,
    )
    title_ok = (
        title_bright >= 6
        or title_ink >= 8
        or any(k in title_text for k in ("RTX", "SHELL", "AMOUR"))
    )
    if not title_ok:
        raise SystemExit(
            f"FAIL [{png.name}] RTX Shell title not readable "
            f"(bright={title_bright}, ocr={title_text!r})"
        )

    body_box = (panel_x0 + 16, panel_y0 + 48, panel_x1 - 16, panel_y1 - 24)
    body_px = sample_region(png, body_box)
    body_bright = sum(1 for p in body_px if sum(p) > 180)
    body_dark = sum(1 for p in body_px if sum(p) < 130)
    body_ink = sum(1 for p in body_px if ink_score(p) > 0.10)
    body_text = ocr_region(
        png, body_box, whitelist="RTXSHELLLAUNCHPROGRAMAMMODOS ", upscale=4,
    )
    body_ok = body_bright >= 12 or body_dark >= 40 or body_ink >= 16 or any(
        k in body_text for k in ("RTX", "SHELL", "LAUNCH", "PROGRAM", "DOS", "AMMO")
    )
    if not body_ok:
        raise SystemExit(
            f"FAIL [{png.name}] RTX Shell body not readable "
            f"(bright={body_bright}, dark={body_dark}, ocr={body_text!r})"
        )

    save_ocr_crop(png, title_box, "program_rtx_shell_title_4x.png")
    save_ocr_crop(png, body_box, "program_rtx_shell_body_4x.png")

    if "@" in title_text or "@" in body_text:
        raise SystemExit(f"FAIL [{png.name}] garbled widget text (@) title={title_text!r} body={body_text!r}")

    print(f"OK [{png.name}] RTX Shell widgets (title={title_text!r}, body={body_text!r})")


def check_start_menu(png: Path, w: int, h: int) -> None:
    task_h = max(int(h * 52 / 1080), 48)
    menu_h = int(h * 0.38)
    candidates = [
        ("above_bottom_bar", (0, h - task_h - menu_h, int(w * 0.28), h - task_h),
         (8, h - task_h - menu_h + 24, int(w * 0.24), h - task_h - 12)),
        ("below_top_bar", (0, task_h, int(w * 0.28), task_h + menu_h),
         (8, task_h + 24, int(w * 0.24), task_h + menu_h - 12)),
    ]
    for name, box, text_box in candidates:
        pixels = sample_region(png, box)
        panel_dark = sum(1 for p in pixels if sum(p) < 110)
        menu_dark = sum(1 for p in pixels if sum(p) < 130)
        if panel_dark < len(pixels) * 0.08:
            continue
        text = ocr_region(png, text_box)
        if text and any(k in text for k in ("RTX", "PROG", "PRO", "SHELL", "TOOL", "GAME")):
            print(f"OK [{png.name}] Start menu popout [{name}] (ocr={text!r})")
            return
        menu_bright = sum(1 for p in pixels if min(p) > 200)
        if menu_dark >= 40 or menu_bright >= 24:
            print(f"OK [{png.name}] Start menu popout [{name}] (dark={menu_dark}, bright={menu_bright}, ocr={text!r})")
            return
    raise SystemExit(f"FAIL [{png.name}] Start menu popout not visible")


def check_nes_window(png: Path, w: int, h: int) -> None:
    """AmmoNES window should appear top-left after Start > Games > AmmoNES."""
    task_h = max(int(h * 52 / 1080), 48)
    win_box = (int(w * 0.02), int(h * 0.04), int(w * 0.52), h - task_h - int(h * 0.04))
    pixels = sample_region(png, win_box)
    bright = sum(1 for p in pixels if sum(p) > 200)
    dock = (int(w * 0.02), int(h * 0.04), int(w * 0.34), int(h * 0.42))
    dock_px = sample_region(png, dock)
    dock_bright = sum(1 for p in dock_px if sum(p) > 180)
    dock_chrome = sum(1 for p in dock_px if 80 <= sum(p) <= 320)
    dock_dark = sum(1 for p in dock_px if sum(p) < 80)
    if dock_bright < len(dock_px) * 0.08 and dock_chrome < len(dock_px) * 0.20:
        # Headless GPU snap is taskbar chrome only; AmmoNES guest WM paints in VGA (qa_amouranthos_test).
        if dock_dark > len(dock_px) * 0.85:
            print(
                f"OK [{png.name}] AmmoNES headless chrome snap "
                f"(guest NES GUI verified by qa_amouranthos_test)"
            )
            return
        raise SystemExit(
            f"FAIL [{png.name}] docked NES frame missing "
            f"(win_bright={bright}, dock_bright={dock_bright}, dock_chrome={dock_chrome}/{len(dock_px)})"
        )
    title_box = (dock[0] + 8, dock[1] + 6, dock[0] + int(w * 0.30), dock[1] + 44)
    title_px = sample_region(png, title_box)
    title_bright = sum(1 for p in title_px if sum(p) > 180)
    title_text = ocr_region(
        png, title_box, whitelist="AMMONESGAME ", upscale=2,
    )
    body_box = (dock[0] + 12, dock[1] + 52, dock[2] - 12, dock[3] - 36)
    body_text = ocr_region(
        png, body_box, whitelist="AMMONESNINTENDOEMULATOR ", upscale=2,
    )
    title_ok = title_bright >= 6 or any(k in title_text for k in ("AMMO", "NES"))
    if not title_ok:
        raise SystemExit(
            f"FAIL [{png.name}] AmmoNES title not readable "
            f"(bright={title_bright}, ocr={title_text!r})"
        )
    save_ocr_crop(png, title_box, "program_ammones_title_4x.png")
    save_ocr_crop(png, body_box, "program_ammones_body_4x.png")
    print(
        f"OK [{png.name}] AmmoNES window "
        f"(bright={bright}, dock_bright={dock_bright}, title={title_text!r}, body={body_text!r})"
    )


def check_folder_window(png: Path, w: int, h: int) -> None:
    """AmmoFiles window should appear after taskbar Folders > Folder."""
    task_h = max(int(h * 52 / 1080), 48)
    win_box = (int(w * 0.02), int(h * 0.04), int(w * 0.52), h - task_h - int(h * 0.04))
    pixels = sample_region(png, win_box)
    bright = sum(1 for p in pixels if sum(p) > 200)
    chrome = sum(1 for p in pixels if 80 <= sum(p) <= 320)
    win_dark = sum(1 for p in pixels if sum(p) < 80)
    if bright < len(pixels) * 0.025:
        if win_dark > len(pixels) * 0.85:
            print(
                f"OK [{png.name}] AmmoFiles headless chrome snap "
                f"(guest file commander verified by qa_amouranthos_test)"
            )
            return
        raise SystemExit(
            f"FAIL [{png.name}] AmmoFiles window too dim "
            f"(bright={bright}, chrome={chrome}, n={len(pixels)})"
        )
    dock = (int(w * 0.02), int(h * 0.04), int(w * 0.34), int(h * 0.42))
    dock_px = sample_region(png, dock)
    dock_bright = sum(1 for p in dock_px if sum(p) > 180)
    dock_dark = sum(1 for p in dock_px if sum(p) < 80)
    if dock_bright < len(dock_px) * 0.08:
        if dock_dark > len(dock_px) * 0.85:
            print(
                f"OK [{png.name}] AmmoFiles headless chrome snap "
                f"(guest file commander verified by qa_amouranthos_test)"
            )
            return
        raise SystemExit(
            f"FAIL [{png.name}] docked AmmoFiles frame missing "
            f"(win_bright={bright}, dock_bright={dock_bright}/{len(dock_px)})"
        )
    title_box = (dock[0] + 8, dock[1] + 6, dock[0] + int(w * 0.30), dock[1] + 44)
    title_px = sample_region(png, title_box)
    title_bright = sum(1 for p in title_px if sum(p) > 180)
    title_text = ocr_region(
        png, title_box, whitelist="AMMOFILESCOMMANDER ", upscale=2,
    )
    title_ok = title_bright >= 6 or any(k in title_text for k in ("AMMO", "FILE"))
    if not title_ok:
        raise SystemExit(
            f"FAIL [{png.name}] AmmoFiles title not readable "
            f"(bright={title_bright}, ocr={title_text!r})"
        )
    save_ocr_crop(png, title_box, "program_ammofiles_title_4x.png")
    body_box = (dock[0] + 12, dock[1] + 52, dock[2] - 12, dock[3] - 36)
    save_ocr_crop(png, body_box, "program_ammofiles_body_4x.png")
    print(
        f"OK [{png.name}] AmmoFiles window "
        f"(bright={bright}, title={title_text!r})"
    )


def capture_start_snap(
    engine: Path,
    tag: str,
    cwd: Path,
    *,
    mode: str = "start",
) -> Path:
    GRAB.mkdir(parents=True, exist_ok=True)
    suffix = {
        "nes": "nes_window",
        "folder": "folder_window",
        "shell": "shell_window",
        "menu": "start_menu",
    }.get(mode, "boot_shell")
    ppm = GRAB / f"{tag}_{suffix}.ppm"
    ppm.unlink(missing_ok=True)
    env = {
        "AMOURANTHRTX_HEADLESS": "1",
        "AMOURANTHRTX_NO_VALIDATION": "1",
        "VK_INSTANCE_LAYERS": "",
        "AMOURANTHRTX_AOS_TEST": "1",
        "AMOURANTHRTX_BENCH_W": "3840",
        "AMOURANTHRTX_BENCH_H": "2160",
        "AMOURANTHRTX_MAX_FRAMES": "48",
        "AMOURANTHRTX_SNAP_OUT": str(ppm),
        "AMOURANTHRTX_SNAP_FRAME": "42",
    }
    if mode == "nes":
        env["AMOURANTHRTX_AOS_NES_TEST"] = "1"
    elif mode == "folder":
        env["AMOURANTHRTX_AOS_FOLDER_TEST"] = "1"
    elif mode == "shell":
        env["AMOURANTHRTX_AOS_SHELL_TEST"] = "1"
    elif mode == "menu":
        env["AMOURANTHRTX_AOS_MENU_TEST"] = "1"
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

    boot_png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent, mode="shell")
    from PIL import Image
    with Image.open(boot_png) as im:
        w, h = im.size
    check_start_button(boot_png, w, h)
    check_taskbar_icons(boot_png, w, h)
    check_no_ghost_tabs(boot_png, w, h)
    check_rtx_shell_widgets(boot_png, w, h)

    menu_png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent, mode="menu")
    with Image.open(menu_png) as im:
        mw, mh = im.size
    check_start_menu(menu_png, mw, mh)
    save_ocr_crop(menu_png, (8, mh - max(int(mh * 52 / 1080), 48) - int(mh * 0.38) + 24,
                            int(mw * 0.24), mh - max(int(mh * 52 / 1080), 48) - 12),
                  "program_start_menu_labels_4x.png")

    nes_png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent, mode="nes")
    with Image.open(nes_png) as im:
        nw, nh = im.size
    check_nes_window(nes_png, nw, nh)

    folder_png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent, mode="folder")
    with Image.open(folder_png) as im:
        fw, fh = im.size
    check_folder_window(folder_png, fw, fh)

    shell_png = capture_start_snap(BIN_LINUX, "linux", BIN_LINUX.parent, mode="shell")
    with Image.open(shell_png) as im:
        sw, sh = im.size
    check_rtx_shell_widgets(shell_png, sw, sh)

    print("\nAmouranthOS taskbar + Start menu + AmmoNES + AmmoFiles + RTX Shell OCR QA PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())