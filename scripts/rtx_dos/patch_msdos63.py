#!/usr/bin/env python3
"""Patch Microsoft MS-DOS 4.00 MIT binaries → RTX-DOS 7.0 GPU Super-Engine."""

from __future__ import annotations

import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from rtx_dos_brand import (  # noqa: E402
    COMMAND_BANNER_BIN,
    DOS_VERSION_BIN,
    LINEAGE,
    PRODUCT,
    VERSION_FULL,
)

BASE = ROOT / "assets" / "dos" / "msdos-base" / "v4.0-ozzie" / "bin" / "DISK1"
V2 = ROOT / "assets" / "dos" / "msdos-base" / "v2.0" / "bin"
OUT = ROOT / "assets" / "dos" / "ammo"

VERSION_DOS = DOS_VERSION_BIN
COMMAND_BANNER = COMMAND_BANNER_BIN


def patch_at(data: bytearray, old: bytes, new: bytes, label: str) -> None:
    idx = data.find(old)
    if idx < 0:
        raise SystemExit(f"{label}: missing {old!r}")
    if len(new) > len(old):
        raise SystemExit(f"{label}: replacement longer than original")
    data[idx:idx + len(old)] = new.ljust(len(old), b" ")


def patch_version_words(data: bytearray) -> None:
    for sig in (b"\x04\x00", b"\x00\x04"):
        off = 0
        while True:
            off = data.find(sig, off)
            if off < 0 or off > 20000:
                break
            if off + 4 <= len(data):
                data[off] = 0x07
                data[off + 1] = 0x00
            off += 1


def copy_utils() -> None:
    pairs = [
        (BASE / "DOS33" / "FDISK.COM", "FDISK.EXE"),
        (BASE / "DOS33" / "FORMAT.COM", "FORMAT.COM"),
        (BASE / "DOS33" / "SYS.COM", "SYS.COM"),
        (V2 / "CHKDSK.COM", "CHKDSK.COM"),
        (V2 / "MORE.COM", "MORE.COM"),
        (V2 / "PRINT.COM", "PRINT.COM"),
        (V2 / "DISKCOPY.COM", "DISKCOPY.COM"),
        (V2 / "DEBUG.COM", "DEBUG.COM"),
        (V2 / "EDLIN.COM", "EDLIN.COM"),
        (V2 / "RECOVER.COM", "RECOVER.COM"),
        (BASE / "BIN" / "CHKDSK.EXE", "CHKDSK.EXE"),
    ]
    dos_dir = OUT / "DOS"
    dos_dir.mkdir(exist_ok=True)
    for src, name in pairs:
        if src.is_file():
            shutil.copy2(src, OUT / name)
            shutil.copy2(src, dos_dir / name)


def main() -> int:
    if not BASE.is_dir():
        raise SystemExit(
            f"MS-DOS base missing ({BASE}) — run: python3 scripts/install_dos.py --force"
        )

    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(Path(__file__).parent / "mk_ammo_stubs.py")], check=True)
    subprocess.run([sys.executable, str(Path(__file__).parent / "mk_ammo_devkit.py"), str(OUT)], check=True)
    subprocess.run([sys.executable, str(ROOT / "scripts" / "mk_tools_com.py")], check=True)
    subprocess.run([sys.executable, str(Path(__file__).parent / "gen_command_txt.py"), str(OUT)], check=True)

    io = bytearray((BASE / "IBMBIO.COM").read_bytes())
    dos = bytearray((BASE / "IBMDOS.COM").read_bytes())
    cmd = bytearray((BASE / "COMMAND.COM").read_bytes())

    patch_at(dos, b"MS-DOS version 4.00", VERSION_DOS, "IBMDOS")
    patch_version_words(dos)
    patch_at(cmd, b"Command v. 4.00", COMMAND_BANNER, "COMMAND")

    (OUT / "IO.SYS").write_bytes(io)
    (OUT / "MSDOS.SYS").write_bytes(dos)
    (OUT / "COMMAND.COM").write_bytes(cmd)
    copy_utils()

    config = (
        "REM RTX-AMMOS Field Layer Stack - RTX-DOS 7.0 GPU Super-Engine\r\n"
        "REM L0=RAM L1=VGA L2=FAT L3=DRIVES L4=VIEW L5=AUDIO L6=MSCDEX L7=IO L8=BIOS\r\n"
        "[MENU]\r\n"
        "MENUITEM=DOS, RTX-DOS 7.0 Command Prompt\r\n"
        "MENUITEM=WIN31, Windows 3.1 / 3.11\r\n"
        "MENUITEM=WIN95, Windows 95\r\n"
        "MENUITEM=WIN98, Windows 98\r\n"
        "MENUCOLOR=7,0\r\n"
        "MENUDEFAULT=DOS,15\r\n"
        "[COMMON]\r\n"
        "REM Supercore L0 - INT 2Fh field mux (RTXKERNEL.SYS)\r\n"
        "DEVICE=C:\\RTXKERNEL.SYS\r\n"
        "REM Layer L0 - XMS + floppy field driver (A: RTXBOOT)\r\n"
        "DEVICE=C:\\HIMEM.SYS\r\n"
        "DEVICE=C:\\RTXDRV.SYS\r\n"
        "REM Layer L1 - VGA/VESA field driver\r\n"
        "DEVICE=C:\\RTXVGA.SYS\r\n"
        "REM Layer L5 - SB16/OPL/GUS audio rack\r\n"
        "DEVICE=C:\\RTXSB.SYS\r\n"
        "REM Layer L3 - CD-ROM block driver (MSCDEX redirector D:)\r\n"
        "DEVICE=C:\\RTXCD.SYS /D:RTXCD001\r\n"
        "REM Layer L2 - GPU-native FAT16+ filesystem (AMMOFAT v1 C:)\r\n"
        "DEVICE=C:\\AMMOFAT.SYS\r\n"
        "REM Layer L6 - Host bridge incoming files (E:)\r\n"
        "DEVICE=C:\\RTXHOST.SYS\r\n"
        "REM UMB host - EMS stub for upper memory blocks\r\n"
        "DEVICE=C:\\EMM386.EXE NOEMS X=0xE000 FRAME=NONE\r\n"
        "DOS=HIGH,UMB,UMBNOAUTO\r\n"
        "FILES=255\r\n"
        "BUFFERS=60,3\r\n"
        "LASTDRIVE=Z\r\n"
        "FCBS=16,0\r\n"
        "STACKS=18,512\r\n"
        "SET TEMP=C:\\TEMP\r\n"
        "SET RTXAMMOS=GPU\r\n"
        "SET RTXLAYER=10\r\n"
        "SET RTXDRIVES=ACDE\r\n"
        "SET INCLUDE=C:\\AMMOINC\r\n"
        "SET PATH=C:\\;C:\\DOS;C:\\TOOLS;C:\\WINDOWS;C:\\WIN\r\n"
        "SET PROMPT=$P$G\r\n"
        "COUNTRY=001,,C:\\COUNTRY.SYS\r\n"
        "[DOS]\r\n"
        "[WIN31]\r\n"
        "CALL C:\\BOOT31.BAT\r\n"
        "[WIN95]\r\n"
        "CALL C:\\BOOT95.BAT\r\n"
        "[WIN98]\r\n"
        "CALL C:\\BOOT98.BAT\r\n"
    )
    autoexec = (
        "@ECHO OFF\r\n"
        "SET RTXAMMOS=GPU\r\n"
        "SET RTXLAYER=10\r\n"
        "SET RTXDRIVES=ACDE\r\n"
        "PROMPT $P$G\r\n"
        "PATH C:\\;C:\\DOS;C:\\TOOLS;C:\\WINDOWS;C:\\WIN\r\n"
        "SET INCLUDE=C:\\AMMOINC\r\n"
        "VER\r\n"
        "SMARTDRV.EXE /Q /N\r\n"
        "ECHO.\r\n"
        "ECHO RTX-AMMOS GPU-only Field Die - Vulkan + SDL3 (no host CPU)\r\n"
        "ECHO Field layers: drives A/C/D/E + AMMOFAT L2 + MSCDEX L3 - TYPE FIELDLAY.TXT\r\n"
        "MSCDEX /D:RTXCD001 /L:D /M:8\r\n"
        "IF EXIST C:\\FIELDLAY.TXT TYPE C:\\FIELDLAY.TXT\r\n"
        "ECHO.\r\n"
        "ECHO Home key = DOS panel 2x / fullscreen 4:3 toggle\r\n"
        "ECHO DevKit: TOOLS AMMOASM AMMOCC AMMOLINK AMMODECOMP BUILD AMMORUN AMMODBG\r\n"
        "ECHO Registry: HKRTX hive C:\\TOOLS\\RTXREG.INI — REG QUERY | REGEDIT\r\n"
        "SET BLASTER=A220 I5 D1 H5 T6\r\n"
        "ECHO Help: COMMAND /?   HELP topic   TYPE COMMAND.TXT\r\n"
        "ECHO Windows: copy to C:\\WINDOWS or use install_rtx_windows.py\r\n"
    )
    country = (
        "RTX-DOS 7.0 Country Code 001\r\n"
        "Date format MM-DD-YYYY\r\n"
        "Time format HH:MM:SS\r\n"
    )
    win_txt = (
        "Windows 3.1 / 95 / 98 dual-boot with RTX-DOS 7.0.\r\n"
        "Copy Windows setup to C:\\WINDOWS then run BOOT31.BAT or BOOT95.BAT\r\n"
        "SET WINBOOTDIR=C:\\WINDOWS  SET WINDIR=C:\\WINDOWS\r\n"
        "Setup: Zachary Geurts MCSE+I — TYPE WIN31.TXT | SETUP31\r\n"
    )
    win31_txt = (
        "Windows 3.1 / 3.11 on RTX-DOS 7.0\r\n"
        "Setup guide — Zachary Geurts MCSE+I\r\n"
        "========================================\r\n"
        "\r\n"
        "1. Copy your licensed Windows 3.1x disks to:\r\n"
        "   assets/dos/incoming/win31/\r\n"
        "\r\n"
        "2. On Linux host:\r\n"
        "   ./linux.sh win31 --stage\r\n"
        "   ./linux.sh dos --force\r\n"
        "\r\n"
        "3. In Field Die DOS shell:\r\n"
        "   SETUP31   — verify C:\\WINDOWS checklist\r\n"
        "   WIN31     — RTX desktop preview\r\n"
        "\r\n"
        "4. Dual-boot: CONFIG.SYS menu Windows 3.1 / 3.11\r\n"
        "\r\n"
        "Required in C:\\WINDOWS:\r\n"
        "  WIN.COM KERNEL.EXE (or KRNL386.EXE) USER.EXE\r\n"
        "  GDI.EXE PROGMAN.EXE SYSTEM.INI\r\n"
        "\r\n"
        "RTX-AMMOS does not ship Microsoft Windows.\r\n"
    )
    boot31 = (
        "@ECHO OFF\r\n"
        "SET WINBOOTDIR=C:\\WINDOWS\r\n"
        "SET WINDIR=C:\\WINDOWS\r\n"
        "IF EXIST C:\\WINDOWS\\WIN.COM GOTO CHECKKERN\r\n"
        "ECHO.\r\n"
        "ECHO Windows 3.1 — Zachary Geurts MCSE+I\r\n"
        "ECHO Copy licensed Windows to C:\\WINDOWS — see WIN31.TXT\r\n"
        "IF EXIST C:\\WIN31.TXT TYPE C:\\WIN31.TXT\r\n"
        "ECHO.\r\n"
        "ECHO RTX preview at C:\\> : WIN31 or SETUP31\r\n"
        "GOTO END\r\n"
        ":CHECKKERN\r\n"
        "IF EXIST C:\\WINDOWS\\KERNEL.EXE GOTO RUN31\r\n"
        "IF EXIST C:\\WINDOWS\\KRNL386.EXE GOTO RUN31\r\n"
        "ECHO KERNEL.EXE / KRNL386.EXE missing in C:\\WINDOWS\r\n"
        "IF EXIST C:\\WIN31.TXT TYPE C:\\WIN31.TXT\r\n"
        "GOTO END\r\n"
        ":RUN31\r\n"
        "CD C:\\WINDOWS\r\n"
        "WIN /S:C\r\n"
        ":END\r\n"
    )
    boot95 = (
        "@ECHO OFF\r\n"
        "SET WINBOOTDIR=C:\\WINDOWS\r\n"
        "IF EXIST C:\\WINDOWS\\WIN.COM GOTO RUN95\r\n"
        "IF EXIST C:\\WINDOWS\\SYSTEM\\WIN.COM GOTO RUN95\r\n"
        "ECHO Copy Windows 95 to C:\\WINDOWS then retry.\r\n"
        "GOTO END\r\n"
        ":RUN95\r\n"
        "CD C:\\WINDOWS\r\n"
        "WIN\r\n"
        ":END\r\n"
    )
    boot98 = (
        "@ECHO OFF\r\n"
        "SET WINBOOTDIR=C:\\WINDOWS\r\n"
        "IF EXIST C:\\WINDOWS\\WIN.COM GOTO RUN98\r\n"
        "IF EXIST C:\\WINDOWS\\SYSTEM\\WIN.COM GOTO RUN98\r\n"
        "ECHO Copy Windows 98 to C:\\WINDOWS then retry.\r\n"
        "ECHO Or type WIN98 at C:\\> for RTX desktop preview.\r\n"
        "GOTO END\r\n"
        ":RUN98\r\n"
        "CD C:\\WINDOWS\r\n"
        "WIN\r\n"
        ":END\r\n"
    )
    win_com = bytes([
        0xEB, 0x0E, 0x90,
        0x57, 0x49, 0x4E, 0x2E, 0x43, 0x4F, 0x4D, 0x00,
        0xB4, 0x09, 0xBA, 0x10, 0x01, 0xCD, 0x21, 0xC3,
        0x52, 0x54, 0x58, 0x2D, 0x44, 0x4F, 0x53, 0x20, 0x57, 0x69, 0x6E, 0x0D, 0x0A, 0x24,
    ])
    msdos_ini = (
        "[Paths]\r\n"
        "WinDir=C:\\WINDOWS\r\n"
        "WinBootDir=C:\\WINDOWS\r\n"
        "[Options]\r\n"
        "BootMulti=1\r\n"
        "BootGUI=0\r\n"
        "Network=0\r\n"
        "Logo=0\r\n"
    )
    (OUT / "AMMOFAT.TXT").write_text(
        "AMMOFAT v1.0 RTX-AMMOS GPU-native FAT16+\r\n"
        "Field layer L2 | signature AMMOFAT1 | IOCTL 0x2F10\r\n"
        "FAT16+ journaling metadata | GPU HD mirror\r\n",
        encoding="ascii",
    )
    (OUT / "FIELDLAY.TXT").write_text(
        "RTX-AMMOS Field Layer Stack (GPU-only Field Die)\r\n"
        "================================================\r\n"
        "L0 RAM      Supercore mux (RTXKERNEL.SYS) + XMS (HIMEM.SYS) + floppy A: (RTXDRV.SYS)\r\n"
        "L1 VGA      IBM VGA + VESA viewport (RTXVGA.SYS)\r\n"
        "L2 FAT      GPU-native FAT16+ C: RTXDOS (AMMOFAT.SYS)\r\n"
        "L3 DRIVES   Field drive rack A/B/C/D/E summary (shell DRIVES)\r\n"
        "L4 VIEWPORT DOS font scale + scanlines (GPU panel)\r\n"
        "L5 AUDIO    SB16/OPL/GUS audio rack (RTXSB.SYS)\r\n"
        "L6 MSCDEX   CD-ROM redirector drive D: (RTXCD.SYS /D:RTXCD001)\r\n"
        "L7 IO       Host bridge E: incoming (RTXHOST.SYS)\r\n"
        "L8 BIOS     RTX char IOCTL bridge (RTXAPI.SYS, optional)\r\n"
        "\r\n"
        "Drive letters: A=floppy B=reserved C=RTXDOS D=CD E=host bridge\r\n"
        "Boot order (CONFIG.SYS [COMMON]):\r\n"
        "  RTXKERNEL.SYS -> HIMEM.SYS -> RTXDRV.SYS -> RTXCD.SYS -> AMMOFAT.SYS -> RTXHOST.SYS -> EMM386.EXE\r\n"
        "AUTOEXEC: MSCDEX /D:RTXCD001 /L:D | PATH includes C:\\TOOLS\r\n"
        "Shell: VER FIELD DRIVES AMMOFAT MSCDEX TOOLS DRIVERS HELP\r\n"
        "DevKit: C:\\TOOLS\\AMMOASM AMMOCC AMMOLINK AMMORUN AMMODBG\r\n"
        "Includes: C:\\AMMOINC (RTX.INC RTX.H) | Samples: C:\\SAMPLES\r\n",
        encoding="ascii",
    )
    (OUT / "DRIVES.TXT").write_text(
        "RTX-AMMOS Field Drive Rack\r\n"
        "==========================\r\n"
        "A: floppy RTXBOOT (RTXDRV.SYS)\r\n"
        "B: reserved\r\n"
        "C: RTXDOS fixed disk (AMMOFAT.SYS)\r\n"
        "D: CD-ROM ISO9660 (RTXCD.SYS + MSCDEX)\r\n"
        "E: host bridge - drop files in assets/dos/incoming/host/\r\n"
        "\r\n"
        "Shell: DRIVES | DIR A: | CD D: | MOUNT CD | TYPE FIELDLAY.TXT\r\n",
        encoding="ascii",
    )
    readme = (
        "RTX-AMMOS GPU-only Super DOSBox (Vulkan Field Die)\r\n"
        "4 GB RAM (BIOS) + 2 GB C: RTXDOS - HD image backup on startup.\r\n"
        "Field layers: TYPE FIELDLAY.TXT | drives A/C/D/E + AMMOFAT + MSCDEX.\r\n"
        "Shell: DRIVES DIR E: MOUNT CD - see DRIVES.TXT\r\n"
        "Home = DOS panel 2x / fullscreen 4:3.\r\n"
        "CONFIG.SYS menu: RTX-DOS / Windows 3.1 / 95 / 98.\r\n"
        "DevKit PATH: C:\\TOOLS | INCLUDE: C:\\AMMOINC\r\n"
        "Utilities: FDISK FORMAT CHKDSK SYS MORE PRINT DEBUG EDLIN\r\n"
    )
    (OUT / "CONFIG.SYS").write_text(config, encoding="utf-8")
    (OUT / "AUTOEXEC.BAT").write_text(autoexec, encoding="utf-8")
    (OUT / "COUNTRY.SYS").write_text(country, encoding="utf-8")
    (OUT / "VERSION.TXT").write_text(
        f"{VERSION_FULL}\r\nGPU Super DOSBox | {LINEAGE}\r\n", encoding="utf-8"
    )
    (OUT / "README.TXT").write_text(readme, encoding="utf-8")
    (OUT / "WIN" / "README.TXT").parent.mkdir(exist_ok=True)
    (OUT / "WIN" / "README.TXT").write_text(win_txt, encoding="utf-8")
    (OUT / "WIN31.TXT").write_text(win31_txt, encoding="utf-8")
    (OUT / "BOOT31.BAT").write_text(boot31, encoding="utf-8")
    (OUT / "BOOT95.BAT").write_text(boot95, encoding="utf-8")
    (OUT / "BOOT98.BAT").write_text(boot98, encoding="utf-8")
    (OUT / "WIN.COM").write_bytes(win_com)
    (OUT / "WIN" / "BOOT31.BAT").write_text(boot31, encoding="utf-8")
    (OUT / "WIN" / "BOOT95.BAT").write_text(boot95, encoding="utf-8")
    (OUT / "WIN" / "BOOT98.BAT").write_text(boot98, encoding="utf-8")
    (OUT / "WINDOWS" / "SYSTEM.INI").parent.mkdir(exist_ok=True)
    (OUT / "WINDOWS" / "SYSTEM.INI").write_text(
        "[boot]\r\noemfonts.fon=vgaoem.fon\r\n", encoding="utf-8"
    )
    (OUT / "MSDOS.INI").write_text(msdos_ini, encoding="utf-8")

    manifest = {"product": VERSION_FULL, "files": {}}
    for path in sorted(OUT.rglob("*")):
        if path.is_file() and path.name != "ammo_manifest.json":
            rel = path.relative_to(OUT).as_posix()
            data = path.read_bytes()
            manifest["files"][rel] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    (OUT / "ammo_manifest.json").write_text(
        __import__("json").dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"patched {VERSION_FULL} → {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())