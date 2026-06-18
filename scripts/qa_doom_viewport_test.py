#!/usr/bin/env python3
"""Verify mode-13h viewport mapping keeps full 320x200 drawable (no vertical smush)."""

from __future__ import annotations

DOOM_W = 320
DOOM_H = 200
PIXEL_SCALE = 2.0


def mode13_uv(local_x: float, local_y: float, vp_w: float, vp_h: float) -> tuple[float, float, bool]:
    game_w = DOOM_W * PIXEL_SCALE
    game_h = DOOM_H * PIXEL_SCALE
    scale = min(vp_w / game_w, vp_h / game_h)
    draw_w = game_w * scale
    draw_h = game_h * scale
    pad_x = (vp_w - draw_w) * 0.5
    pad_y = (vp_h - draw_h) * 0.5
    lp_x = local_x * vp_w
    lp_y = local_y * vp_h
    if lp_x < pad_x or lp_x >= vp_w - pad_x or lp_y < pad_y or lp_y >= vp_h - pad_y:
        return 0.0, 0.0, True
    gu = (lp_x - pad_x) / max(draw_w, 1.0)
    gv = (lp_y - pad_y) / max(draw_h, 1.0)
    return gu, gv, False


def main() -> int:
    cases = [
        (1188.0, 910.0, "AOS stamp panel"),
        (1920.0, 980.0, "fullscreen stretch"),
        (640.0, 458.0, "logical DOS window"),
    ]
    for vp_w, vp_h, label in cases:
        game_w = DOOM_W * PIXEL_SCALE
        game_h = DOOM_H * PIXEL_SCALE
        scale = min(vp_w / game_w, vp_h / game_h)
        draw_h = game_h * scale
        pad_y = (vp_h - draw_h) * 0.5
        top_local = (pad_y + 1.0) / vp_h
        bot_local = (vp_h - pad_y - 1.0) / vp_h
        mid_local = 0.5
        _, gv_top, _ = mode13_uv(0.5, top_local, vp_w, vp_h)
        _, gv_bot, _ = mode13_uv(0.5, bot_local, vp_w, vp_h)
        draw_w = game_w * scale
        pad_x = (vp_w - draw_w) * 0.5
        left_local = (pad_x + 1.0) / vp_w
        right_local = (vp_w - pad_x - 1.0) / vp_w
        gu_l, _, lb_l = mode13_uv(left_local, mid_local, vp_w, vp_h)
        gu_r, _, _ = mode13_uv(right_local, mid_local, vp_w, vp_h)
        _, _, lb_mid = mode13_uv(0.5, mid_local, vp_w, vp_h)
        assert not lb_mid, f"{label}: mid should be inside game"
        assert not lb_l, f"{label}: left edge inside"
        assert abs(gv_top) < 0.02, f"{label}: top uv={gv_top}"
        assert abs(gv_bot - 1.0) < 0.02, f"{label}: bottom uv={gv_bot}"
        assert abs(gu_l) < 0.02 and abs(gu_r - 1.0) < 0.02, f"{label}: horizontal span"
        draw_h = DOOM_H * PIXEL_SCALE * min(vp_w / (DOOM_W * PIXEL_SCALE), vp_h / (DOOM_H * PIXEL_SCALE))
        frac = draw_h / vp_h
        print(f"PASS {label}: draw_h/vp_h={frac:.3f} (>{0.5:.1f})")
        if frac < 0.35:
            raise SystemExit(f"FAIL {label}: drawable too thin ({frac:.3f})")
    print("PASS doom viewport aspect fit")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())