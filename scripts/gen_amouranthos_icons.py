#!/usr/bin/env python3
"""Generate AmouranthOS RTX 2026 icon atlas (Lucide-inspired open stroke icons, ISC-style).

Outputs:
  assets/data/icons/aos_icon_atlas.png  — 1024×128 RGBA atlas (64×64 cells)
  assets/data/icons/*.png               — individual 64×64 icons
"""

from __future__ import annotations

import math
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFilter
except ImportError:
    raise SystemExit("pip install pillow") from None

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "data" / "icons"
OUT.mkdir(parents=True, exist_ok=True)

CELL = 64
COLS = 16
ROWS = 2
ATLAS_W = CELL * COLS
ATLAS_H = CELL * ROWS

# RTX 2026 palette — rose glass · cyan accent · void
C_VOID = (14, 10, 22, 0)
C_GLASS = (28, 18, 38, 220)
C_ROSE = (235, 72, 128, 255)
C_CYAN = (72, 220, 255, 255)
C_GLOW = (255, 180, 210, 180)
C_STROKE = (248, 242, 255, 255)
C_MUTED = (160, 140, 190, 200)


def blank_cell() -> Image.Image:
    img = Image.new("RGBA", (CELL, CELL), C_VOID)
    return img


def glass_back(img: Image.Image, tint: tuple[int, int, int, int] = C_GLASS) -> None:
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((4, 4, CELL - 5, CELL - 5), radius=14, fill=tint)
    d.rounded_rectangle((6, 6, CELL - 7, CELL - 7), radius=12, outline=(*C_ROSE[:3], 90), width=1)
    glow = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((8, 8, CELL - 8, CELL - 8), fill=(*C_GLOW[:3], 40))
    img.alpha_composite(glow)


def stroke(draw: ImageDraw.ImageDraw, pts: list[tuple[float, float]], color=C_STROKE, w: int = 3) -> None:
    if len(pts) < 2:
        return
    draw.line(pts, fill=color, width=w, joint="curve")


def icon_start() -> Image.Image:
    img = blank_cell()
    glass_back(img, (48, 14, 32, 230))
    d = ImageDraw.Draw(img)
    cx, cy = CELL // 2, CELL // 2
    d.ellipse((cx - 18, cy - 18, cx + 18, cy + 18), outline=C_ROSE, width=3)
    d.ellipse((cx - 12, cy - 12, cx + 12, cy + 12), fill=(*C_ROSE[:3], 60))
    d.text((cx - 7, cy - 11), "A", fill=C_STROKE)
    return img


def icon_close() -> Image.Image:
    img = blank_cell()
    glass_back(img, (42, 12, 28, 210))
    d = ImageDraw.Draw(img)
    m = 18
    stroke(d, [(m, m), (CELL - m, CELL - m)], C_STROKE, 4)
    stroke(d, [(CELL - m, m), (m, CELL - m)], C_STROKE, 4)
    return img


def icon_minimize() -> Image.Image:
    img = blank_cell()
    glass_back(img, (22, 28, 48, 210))
    d = ImageDraw.Draw(img)
    y = CELL // 2 + 8
    stroke(d, [(18, y), (CELL - 18, y)], C_CYAN, 4)
    return img


def icon_maximize() -> Image.Image:
    img = blank_cell()
    glass_back(img, (22, 28, 48, 210))
    d = ImageDraw.Draw(img)
    m = 17
    d.rounded_rectangle((m, m, CELL - m, CELL - m), radius=4, outline=C_CYAN, width=3)
    return img


def icon_shell() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((14, 16, CELL - 14, CELL - 14), radius=6, outline=C_CYAN, width=2)
    for i, ch in enumerate([">", "_", "~"]):
        d.text((20, 22 + i * 12), ch, fill=C_STROKE)
    return img


def icon_code() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    stroke(d, [(22, 20), (14, 32), (22, 44)], C_CYAN, 3)
    stroke(d, [(42, 20), (50, 32), (42, 44)], C_ROSE, 3)
    d.line([(28, 46), (36, 18)], fill=C_MUTED, width=2)
    return img


def icon_folder() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((14, 24, CELL - 14, CELL - 16), radius=4, outline=C_CYAN, width=2)
    d.rounded_rectangle((14, 18, 34, 28), radius=3, fill=(*C_CYAN[:3], 120))
    return img


def icon_vscodium() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    # stylized V + fork
    stroke(d, [(20, 18), (32, 46), (44, 18)], C_CYAN, 3)
    d.ellipse((38, 36, 50, 48), outline=C_ROSE, width=2)
    return img


def icon_qbasic() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.text((14, 20), "QB", fill=C_STROKE)
    d.rectangle((14, 38, CELL - 14, CELL - 18), outline=C_MUTED, width=1)
    return img


def icon_chip() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rectangle((22, 22, CELL - 22, CELL - 22), outline=C_CYAN, width=2)
    for i in range(4):
        y = 24 + i * 8
        d.line([(18, y), (22, y)], fill=C_ROSE, width=2)
        d.line([(CELL - 22, y), (CELL - 18, y)], fill=C_ROSE, width=2)
    return img


def icon_gamepad() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((12, 26, CELL - 12, CELL - 18), radius=12, outline=C_CYAN, width=2)
    d.ellipse((22, 34, 30, 42), outline=C_STROKE, width=2)
    d.ellipse((34, 34, 42, 42), outline=C_STROKE, width=2)
    return img


def icon_nes() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((16, 22, CELL - 16, CELL - 20), radius=8, outline=C_ROSE, width=2)
    d.text((26, 28), "NES", fill=C_STROKE)
    return img


def icon_settings() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    cx, cy = CELL // 2, CELL // 2
    for i in range(8):
        ang = i * math.pi / 4
        x1 = cx + math.cos(ang) * 10
        y1 = cy + math.sin(ang) * 10
        x2 = cx + math.cos(ang) * 18
        y2 = cy + math.sin(ang) * 18
        d.line([(x1, y1), (x2, y2)], fill=C_MUTED, width=3)
    d.ellipse((cx - 8, cy - 8, cx + 8, cy + 8), outline=C_CYAN, width=2)
    return img


def icon_power() -> Image.Image:
    img = blank_cell()
    glass_back(img, (36, 14, 24, 210))
    d = ImageDraw.Draw(img)
    cx = CELL // 2
    d.arc((cx - 14, 18, cx + 14, 46), start=200, end=-20, fill=C_ROSE, width=3)
    d.line([(cx, 16), (cx, 34)], fill=C_STROKE, width=3)
    return img


def icon_doom() -> Image.Image:
    img = blank_cell()
    glass_back(img, (52, 10, 18, 235))
    d = ImageDraw.Draw(img)
    d.polygon([(32, 14), (48, 28), (44, 50), (20, 50), (16, 28)], outline=C_ROSE, fill=(*C_ROSE[:3], 90))
    d.rectangle((22, 30, 42, 44), fill=(40, 12, 18, 200))
    for x in (26, 32, 38):
        d.rectangle((x, 34, x + 3, 40), fill=C_CYAN)
    d.text((20, 48), "DM", fill=C_STROKE)
    return img


def icon_wallpaper() -> Image.Image:
    img = blank_cell()
    glass_back(img, (18, 32, 52, 220))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((12, 14, CELL - 12, CELL - 18), radius=6, outline=C_CYAN, width=2)
    d.polygon([(14, 40), (28, 28), (40, 36), (50, 22), (50, CELL - 20), (14, CELL - 20)],
              fill=(*C_CYAN[:3], 80))
    d.ellipse((36, 18, 48, 30), fill=(*C_ROSE[:3], 180))
    return img


def icon_desktop() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((10, 16, CELL - 10, CELL - 22), radius=5, outline=C_CYAN, width=2)
    d.rectangle((10, 16, CELL - 10, 26), fill=(*C_ROSE[:3], 140))
    d.ellipse((14, 30, 26, 42), fill=C_CYAN)
    d.ellipse((14, 46, 26, 58), fill=C_ROSE)
    d.ellipse((14, 62, 26, 74), fill=C_STROKE)
    return img


def icon_ammotext() -> Image.Image:
    img = blank_cell()
    glass_back(img, (24, 28, 48, 225))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((14, 14, CELL - 14, CELL - 16), radius=4, outline=C_CYAN, width=2)
    for y in (22, 28, 34, 40, 46):
        d.line([(18, y), (CELL - 18, y)], fill=C_STROKE if y != 34 else C_ROSE, width=2)
    d.text((20, 48), "TXT", fill=C_CYAN)
    return img


def icon_monitor() -> Image.Image:
    img = blank_cell()
    glass_back(img, (18, 28, 42, 225))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((14, 16, CELL - 14, CELL - 22), radius=4, outline=C_CYAN, width=2)
    for i in range(4):
        h = 12 + i * 10
        d.line([(20, CELL - h), (28 + i * 8, h)], fill=C_ROSE if i % 2 else C_CYAN, width=2)
    d.text((18, 18), "SYS", fill=C_STROKE)
    return img


def icon_browser_firefox() -> Image.Image:
    img = blank_cell()
    glass_back(img, (42, 18, 22, 220))
    d = ImageDraw.Draw(img)
    d.pieslice((10, 14, CELL - 10, CELL - 12), start=200, end=340, fill=(*C_ROSE[:3], 200), outline=C_STROKE, width=2)
    d.pieslice((18, 22, CELL - 18, CELL - 20), start=30, end=160, fill=(*C_CYAN[:3], 180))
    return img


def icon_browser_chrome() -> Image.Image:
    img = blank_cell()
    glass_back(img)
    d = ImageDraw.Draw(img)
    cx, cy = CELL // 2, CELL // 2 + 2
    d.ellipse((cx - 16, cy - 16, cx + 16, cy + 16), outline=C_STROKE, width=2)
    d.pieslice((cx - 14, cy - 14, cx + 14, cy + 14), start=0, end=120, fill=(*C_ROSE[:3], 200))
    d.pieslice((cx - 14, cy - 14, cx + 14, cy + 14), start=120, end=240, fill=(*C_CYAN[:3], 200))
    d.pieslice((cx - 14, cy - 14, cx + 14, cy + 14), start=240, end=360, fill=(90, 200, 120, 200))
    d.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=C_STROKE)
    return img


def icon_browser_edge() -> Image.Image:
    img = blank_cell()
    glass_back(img, (16, 36, 58, 220))
    d = ImageDraw.Draw(img)
    d.pieslice((12, 14, CELL - 12, CELL - 14), start=30, end=210, fill=(*C_CYAN[:3], 210), outline=C_STROKE, width=2)
    d.polygon([(32, 20), (48, 32), (32, 44)], fill=C_STROKE)
    return img


def icon_display() -> Image.Image:
    img = blank_cell()
    glass_back(img, (22, 18, 48, 220))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((12, 16, CELL - 12, CELL - 24), radius=4, outline=C_CYAN, width=2)
    d.rectangle((26, CELL - 22, 38, CELL - 18), fill=C_MUTED)
    d.line([(20, 20), (44, 20)], fill=C_ROSE, width=3)
    d.line([(20, 28), (40, 28)], fill=C_CYAN, width=2)
    return img


# Slot order must match FieldAmouranthTextures.hpp + shader AOS_ICON_* constants
ICON_SLOTS: list[tuple[str, callable]] = [
    ("start", icon_start),
    ("close", icon_close),
    ("minimize", icon_minimize),
    ("maximize", icon_maximize),
    ("shell", icon_shell),
    ("ammocode", icon_code),
    ("filecmd", icon_folder),
    ("vscodium", icon_vscodium),
    ("qbasic", icon_qbasic),
    ("fieldc", icon_chip),
    ("padtest", icon_gamepad),
    ("nes", icon_nes),
    ("settings", icon_settings),
    ("power", icon_power),
    ("doom", icon_doom),
    ("wallpaper", icon_wallpaper),
    ("desktop", icon_desktop),
    ("display", icon_display),
    ("ammotext", icon_ammotext),
    ("monitor", icon_monitor),
    ("browser_firefox", icon_browser_firefox),
    ("browser_chrome", icon_browser_chrome),
    ("browser_edge", icon_browser_edge),
]


def build_atlas() -> Image.Image:
    atlas = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    for idx, (name, fn) in enumerate(ICON_SLOTS):
        col = idx % COLS
        row = idx // COLS
        icon = fn()
        # Also write amouranthos.png from start slot for legacy path
        path = OUT / f"{name}.png"
        icon.save(path)
        print(f"wrote {path}")
        atlas.paste(icon, (col * CELL, row * CELL))
    atlas_path = OUT / "aos_icon_atlas.png"
    atlas.save(atlas_path)
    print(f"wrote {atlas_path} ({ATLAS_W}x{ATLAS_H}, {len(ICON_SLOTS)} icons)")
    # Legacy start orb used by kStartIconPath
    (OUT / "amouranthos.png").write_bytes((OUT / "start.png").read_bytes())
    print(f"synced amouranthos.png ← start.png")
    return atlas


def main() -> int:
    build_atlas()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())