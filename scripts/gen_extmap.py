#!/usr/bin/env python3
"""Generate C:\\TOOLS\\EXTMAP.TXT default extension association table."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA_OUT = ROOT / "assets" / "data" / "registry" / "EXTMAP.TXT"
FAT_OUT = ROOT / "assets" / "dos" / "ammo" / "TOOLS" / "EXTMAP.TXT"

ROWS = [
    (".COM", "", "%1", "DOS command file"),
    (".EXE", "", "%1", "DOS executable"),
    (".BAT", "", "%1", "Batch script"),
    (".SYS", "TYPE", "%1", "System driver"),
    (".TXT", "TYPE", "%1", "Text document"),
    (".DOC", "TYPE", "%1", "Document"),
    (".INI", "EDIT", "%1", "Configuration"),
    (".CFG", "EDIT", "%1", "Configuration"),
    (".ZIP", r"C:\TOOLS\AMMOZIP.COM", "%1", "ZIP archive"),
    (".NES", "NES", "%1", "iNES ROM - AmmoNES"),
    (".ROM", "NES", "%1", "NES ROM - AmmoNES"),
    (".ISO", "MOUNT", "%1", "CD-ROM image"),
    (".IMG", "MOUNT", "%1", "Disk image"),
    (".WAD", r"C:\GAMES\DOOM\DOOM.EXE", "%1", "Doom WAD"),
    (".WL6", r"C:\GAMES\WOLF3D\WOLF3D.EXE", "%1", "Wolf3D data"),
    (".WL1", r"C:\GAMES\WOLF3D\WOLF3D.EXE", "%1", "Wolf3D map/audio"),
    (".CK4", r"C:\GAMES\KEEN4\KEEN4E.EXE", "%1", "Commander Keen 4"),
    (".ASM", "AMMOCODE", "%1", "Assembly source"),
    (".C", "AMMOCODE", "%1", "C source"),
    (".H", "AMMOCODE", "%1", "Header source"),
    (".FLD", "FIELDC", "%1", "Field script"),
    (".BAS", "QBASIC", "%1", "BASIC program"),
    (".WAV", "SOUND", "%1", "Wave audio"),
    (".OBJ", "AMMOLINK", "%1", "Object module"),
    (".HTML", "BROWSER", "%1", "Web page"),
    (".HTM", "BROWSER", "%1", "Web page"),
]


def main() -> int:
    lines = [
        "; RTX-DOS 7.0 Extension Map - ext | handler | args | description",
        "; %1 = file path. Blank handler = run file directly.",
        "",
    ]
    for ext, handler, args, desc in ROWS:
        lines.append(f"{ext} | {handler} | {args} | {desc}")
    text = "\r\n".join(lines) + "\r\n"
    for out in (DATA_OUT, FAT_OUT):
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="ascii")
    print(f"wrote {DATA_OUT} + FAT mirror ({len(ROWS)} associations)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())