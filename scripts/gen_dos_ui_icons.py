#!/usr/bin/env python3
"""Generate RTX-DOS HUD icon PNGs (16x16 pixel art) for assets/dos/ui/."""

from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install pillow") from None

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "dos" / "ui"
OUT.mkdir(parents=True, exist_ok=True)

C_VOID = (18, 22, 32, 255)
C_PANEL = (42, 52, 72, 255)
C_COPPER = (196, 128, 64, 255)
C_VALUE = (96, 196, 255, 255)
C_WARN = (255, 196, 64, 255)
C_TITLE = (220, 228, 240, 255)


def blank() -> Image.Image:
    return Image.new("RGBA", (16, 16), C_VOID)


def save(name: str, img: Image.Image) -> None:
    path = OUT / f"{name}.png"
    img.save(path)
    print(f"wrote {path}")


def icon_memory() -> Image.Image:
    img = blank()
    px = img.load()
    for y in range(3, 13):
        for x in range(4, 12):
            px[x, y] = C_VALUE if 5 <= x <= 10 and 4 <= y <= 11 else C_PANEL
    for x in range(3, 13):
        px[x, 3] = C_COPPER
        px[x, 12] = C_COPPER
    return img


def icon_disk() -> Image.Image:
    img = blank()
    px = img.load()
    cx, cy, r = 8, 8, 5
    for y in range(16):
        for x in range(16):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if r - 1.2 <= d <= r:
                px[x, y] = C_WARN
            elif d < 1.5:
                px[x, y] = C_TITLE
    return img


def icon_drive() -> Image.Image:
    img = blank()
    px = img.load()
    for y in range(4, 13):
        for x in range(3, 13):
            px[x, y] = C_PANEL
    for y in range(6, 9):
        for x in range(5, 11):
            px[x, y] = C_VOID
    px[4, 5] = C_COPPER
    px[11, 5] = C_COPPER
    return img


def icon_zoom() -> Image.Image:
    img = blank()
    px = img.load()
    for i in range(6):
        px[3 + i, 3] = C_VALUE
        px[3, 3 + i] = C_VALUE
        px[12 - i, 12] = C_VALUE
        px[12, 12 - i] = C_VALUE
    for y in range(5, 11):
        for x in range(5, 11):
            px[x, y] = C_PANEL
    return img


def icon_stamp() -> Image.Image:
    img = blank()
    px = img.load()
    for y in range(5, 12):
        for x in range(5, 12):
            px[x, y] = C_PANEL
    for y in range(2, 7):
        for x in range(2, 7):
            if x == 2 or y == 2 or x == 6 or y == 6:
                px[x, y] = C_COPPER
    return img


def main() -> int:
    save("memory", icon_memory())
    save("disk", icon_disk())
    save("drive", icon_drive())
    save("zoom", icon_zoom())
    save("stamp", icon_stamp())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())