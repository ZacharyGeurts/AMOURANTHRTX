#!/usr/bin/env python3
"""Generate C:\\TOOLS\\RTXREG.INI — HKRTX hierarchical registry (2026)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA_OUT = ROOT / "assets" / "data" / "registry" / "RTXREG.INI"
FAT_OUT = ROOT / "assets" / "dos" / "ammo" / "TOOLS" / "RTXREG.INI"
EXTMAP = ROOT / "assets" / "data" / "registry" / "EXTMAP.TXT"
if not EXTMAP.is_file():
    EXTMAP = ROOT / "assets" / "dos" / "ammo" / "TOOLS" / "EXTMAP.TXT"

SECTIONS: dict[str, dict[str, str]] = {
    r"HKRTX\Machine\Memory": {
        "ReportedRamGB": "4",
        "GuestFastMB": "1",
        "BootConventionalKB": "512",
        "ConventionalKB": "512",
        "MaxConventionalKB": "640",
    },
    r"HKRTX\Machine\Storage": {
        "HdLogicalGB": "4",
        "RaidStripeKB": "64",
        "RaidTickBudgetKB": "256",
    },
    r"HKRTX\Software\RTX-DOS": {
        "Version": "7.0",
        "Build": "2026",
    },
    r"HKRTX\Software\RTX-DOS\Shell": {
        "Path": r"C:\;C:\DOS;C:\TOOLS;C:\WINDOWS;C:\WIN;C:\AMMOCODE;C:\QBASIC;C:\SOUND",
        "Prompt": "$P$G",
    },
    r"HKRTX\Software\RTX-DOS\Audio": {
        "SB16": "1",
        "GUS": "1",
        "Mouse": "1",
        "Joystick": "1",
    },
    r"HKRTX\User\Desktop": {
        "CommanderMouse": "1",
        "ExtensionEditor": "F6",
    },
}

LAYERS = ("ram", "vga", "fat", "drives", "viewport", "audio", "mscdex", "input", "io", "bios")
for ly in LAYERS:
    SECTIONS[rf"HKRTX\Layers\{ly}"] = {"Enabled": "1"}


def parse_extmap(path: Path) -> None:
    if not path.is_file():
        return
    for raw in path.read_text(encoding="ascii", errors="replace").splitlines():
        line = raw.strip()
        if not line or line[0] in ";#":
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 2:
            continue
        ext = parts[0].upper()
        if not ext.startswith("."):
            ext = "." + ext
        sec = rf"HKRTX\Associations\{ext}"
        SECTIONS[sec] = {
            "Handler": parts[1],
            "Args": parts[2] if len(parts) > 2 else "%1",
            "Desc": parts[3] if len(parts) > 3 else "",
        }


def main() -> int:
    parse_extmap(EXTMAP)
    lines = [
        "; RTX Registry 2026 v1 - HKRTX hierarchical configuration",
        "; Sections map to registry paths. Journal: RTXREG.JRN",
        "; Associations sync to EXTMAP.TXT on save.",
        "",
    ]
    for sec in sorted(SECTIONS):
        lines.append(f"[{sec}]")
        for key in sorted(SECTIONS[sec]):
            lines.append(f"{key}={SECTIONS[sec][key]}")
        lines.append("")
    text = "\r\n".join(lines) + "\r\n"
    for out in (DATA_OUT, FAT_OUT):
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="ascii")
    print(f"wrote {DATA_OUT} + FAT mirror ({len(SECTIONS)} sections)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())