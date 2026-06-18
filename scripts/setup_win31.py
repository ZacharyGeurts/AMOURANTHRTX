#!/usr/bin/env python3
"""Windows 3.1 / 3.11 setup for RTX-DOS 7.0 — Zachary Geurts MCSE+I checklist.

Drop licensed Windows 3.1x files into assets/dos/incoming/win31/ then run:
  ./linux.sh win31
  ./linux.sh win31 --stage   (copy incoming → ammo/WINDOWS, rebuild HD)
  ./linux.sh win31 --check   (report only)

We do not redistribute Windows; you supply your own install media.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCOMING = ROOT / "assets" / "dos" / "incoming" / "win31"
AMMO = ROOT / "assets" / "dos" / "ammo"
WINDOWS = AMMO / "WINDOWS"
SCRIPTS = ROOT / "scripts"

AUTHOR = "Zachary Geurts MCSE+I"
RTX_STUB_WIN_COM_BYTES = 35

# Core files for a runnable Windows 3.1x session (KERNEL or KRNL386 for 3.11).
REQUIRED = (
    ("WIN.COM", "Windows shell launcher"),
    ("KERNEL.EXE", "16-bit kernel (3.1x) — or KRNL386.EXE for 3.11"),
    ("USER.EXE", "User module"),
    ("GDI.EXE", "Graphics device interface"),
    ("PROGMAN.EXE", "Program Manager"),
    ("SYSTEM.INI", "System configuration"),
)

OPTIONAL = (
    ("KRNL386.EXE", "386 enhanced kernel (3.11)"),
    ("WIN.INI", "User configuration"),
    ("WINHELP.EXE", "Help engine"),
    ("SHELL.DLL", "Shell DLL"),
    ("MSDOS.EXE", "DOS box"),
    ("CONTROL.EXE", "Control Panel"),
    ("FONTS.FON", "System fonts"),
    ("VGAOEM.FON", "OEM VGA font"),
)


def is_real_win_com(path: Path) -> bool:
    if not path.is_file():
        return False
    return path.stat().st_size > RTX_STUB_WIN_COM_BYTES


def find_file(name: str, base: Path) -> Path | None:
    direct = base / name
    if direct.is_file():
        return direct
    upper = base / name.upper()
    if upper.is_file():
        return upper
    lower = base / name.lower()
    if lower.is_file():
        return lower
    for hit in base.rglob(name):
        if hit.is_file():
            return hit
    for hit in base.rglob(name.upper()):
        if hit.is_file():
            return hit
    return None


def check_tree(base: Path, label: str) -> dict:
    rows: list[tuple[str, str, str, Path | None]] = []
    ok = True
    for fname, desc in REQUIRED:
        hit = find_file(fname, base) if base.is_dir() else None
        if fname == "KERNEL.EXE" and not hit:
            hit = find_file("KRNL386.EXE", base) if base.is_dir() else None
            fname = "KRNL386.EXE" if hit else fname
        status = "OK" if hit else "MISSING"
        if status == "MISSING":
            ok = False
        rows.append((fname, desc, status, hit))

    opt_rows: list[tuple[str, str, Path | None]] = []
    for fname, desc in OPTIONAL:
        hit = find_file(fname, base) if base.is_dir() else None
        opt_rows.append((fname, desc, hit))

    win = find_file("WIN.COM", base) if base.is_dir() else None
    real_win = is_real_win_com(win) if win else False

    return {
        "label": label,
        "path": base,
        "rows": rows,
        "optional": opt_rows,
        "ok": ok,
        "real_win_com": real_win,
        "win_com": win,
    }


def print_report(incoming: dict, staged: dict) -> None:
    print(f"\n{'=' * 64}")
    print(f"  Windows 3.1 Setup — {AUTHOR}")
    print(f"  RTX-DOS 7.0 GPU Super DOSBox | Field Die C: RTXDOS")
    print(f"{'=' * 64}\n")

    for rep in (incoming, staged):
        print(f"  [{rep['label']}] {rep['path']}")
        if not rep["path"].is_dir():
            print("    (directory not found)\n")
            continue
        for fname, desc, status, hit in rep["rows"]:
            size = f"  {hit.stat().st_size:>8} bytes" if hit else ""
            print(f"    [{status:7}] {fname:<14} {desc}{size}")
        if rep["win_com"]:
            kind = "licensed" if rep["real_win_com"] else "RTX stub only"
            print(f"    WIN.COM: {kind}")
        print()

    n_opt = sum(1 for _, _, h in staged["optional"] if h)
    print(f"  Optional staged: {n_opt}/{len(staged['optional'])} files in C:\\WINDOWS")
    print()

    if staged["ok"] and staged["real_win_com"]:
        print("  STATUS: READY — CONFIG.SYS [WIN31] or C:\\> WIN31")
        print("  Boot:   menu Windows 3.1 / 3.11 → BOOT31.BAT → WIN /S:C")
    elif staged["ok"]:
        print("  STATUS: FILES OK but WIN.COM is RTX stub — replace with licensed WIN.COM")
    elif incoming["ok"]:
        print("  STATUS: Incoming complete — run: ./linux.sh win31 --stage")
    else:
        print("  STATUS: INCOMPLETE — drop files into:")
        print(f"    {INCOMING}/")
        print("  Then: ./linux.sh win31 --stage")
        print("  Preview desktop without media: C:\\> WIN31  (RTX VGA mode 13h)")
    print()


def write_win31_txt() -> None:
    lines = [
        "Windows 3.1 / 3.11 on RTX-DOS 7.0",
        f"Setup guide - {AUTHOR}",
        "=" * 40,
        "",
        "1. Copy your licensed Windows 3.1x disks to:",
        "   assets/dos/incoming/win31/",
        "",
        "2. On Linux host:",
        "   ./linux.sh win31 --stage",
        "   ./linux.sh dos --force",
        "",
        "3. In Field Die DOS shell:",
        "   SETUP31   — verify C:\\WINDOWS checklist",
        "   WIN31     — RTX desktop preview (no Windows binaries)",
        "",
        "4. Dual-boot:",
        "   CONFIG.SYS menu → Windows 3.1 / 3.11",
        "   or C:\\BOOT31.BAT",
        "",
        "Required in C:\\WINDOWS:",
        "  WIN.COM KERNEL.EXE (or KRNL386.EXE) USER.EXE",
        "  GDI.EXE PROGMAN.EXE SYSTEM.INI",
        "",
        "WINBOOTDIR=C:\\WINDOWS  per MSDOS.INI / SYSTEM.INI",
        "",
        "RTX-AMMOS does not ship Microsoft Windows.",
        "You must supply your own licensed install media.",
        "",
    ]
    (AMMO / "WIN31.TXT").write_text("\r\n".join(lines) + "\r\n", encoding="ascii")


def stage_and_rebuild() -> int:
    script = SCRIPTS / "install_rtx_windows.py"
    subprocess.run([sys.executable, str(script)], cwd=ROOT, check=True)
    write_win31_txt()
    hd = ROOT / "assets" / "dos" / "rtx_dos_hd.img"
    if not hd.is_file():
        from rtx_dos_brand import HD_IMAGE  # noqa: E402

        hd = ROOT / "assets" / "dos" / HD_IMAGE
    if hd.is_file():
        subprocess.run(
            [sys.executable, str(SCRIPTS / "mk_ammo_hd.py"), str(hd)],
            cwd=ROOT,
            check=True,
        )
        print(f"Rebuilt HD: {hd}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Windows 3.1 setup — {AUTHOR}")
    parser.add_argument("--check", action="store_true", help="Report checklist only")
    parser.add_argument("--stage", action="store_true", help="Stage incoming + rebuild HD")
    args = parser.parse_args()

    INCOMING.mkdir(parents=True, exist_ok=True)
    WINDOWS.mkdir(parents=True, exist_ok=True)

    incoming = check_tree(INCOMING, "incoming/win31")
    staged = check_tree(WINDOWS, "C:\\WINDOWS (staged)")
    print_report(incoming, staged)

    if args.check:
        return 0 if staged["ok"] and staged["real_win_com"] else 1

    if args.stage:
        if not any(INCOMING.rglob("*")):
            print("No files in incoming/win31 — add Windows media first.")
            return 1
        return stage_and_rebuild()

    write_win31_txt()
    if incoming["ok"]:
        print("Incoming looks complete. Staging...")
        return stage_and_rebuild()

    (ROOT / "assets" / "dos" / "incoming" / "PLACE_WINDOWS_HERE.txt").write_text(
        "Windows 3.1 / 3.11 (licensed media — not redistributed)\r\n"
        f"Setup: {AUTHOR}\r\n"
        "\r\n"
        "Drop files into win31/:\r\n"
        "  WIN.COM KERNEL.EXE USER.EXE GDI.EXE PROGMAN.EXE SYSTEM.INI\r\n"
        "  (KRNL386.EXE for Windows 3.11)\r\n"
        "\r\n"
        "Then: ./linux.sh win31 --stage && ./linux.sh dos --force\r\n"
        "Shell: SETUP31 | WIN31 (preview)\r\n",
        encoding="ascii",
    )
    return 0 if staged["real_win_com"] else 1


if __name__ == "__main__":
    raise SystemExit(main())