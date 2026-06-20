#!/usr/bin/env python3
"""RTX-DOS 7.0 installer — Modern DOS (Microsoft MS-DOS MIT lineage, no FreeDOS)."""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOS_DIR = ROOT / "assets" / "dos"
AMMO_DIR = DOS_DIR / "ammo"
SCRIPTS = ROOT / "scripts"
MSDOS_BASE = ROOT / "assets" / "dos" / "msdos-base"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ammo_platform import (  # noqa: E402
    FLOPPY_BYTES,
    GUEST_RAM_FAST_MB,
    HD_SIZE_GB,
    HD_SIZE_MB,
    MANIFEST_VERSION,
    PRODUCT_NAME,
    REPORTED_RAM_GB,
)
from rtx_dos_brand import FLOPPY_IMAGE, HD_IMAGE, LINEAGE, TAGLINE  # noqa: E402

HD_BYTES = HD_SIZE_MB * 1024 * 1024

MANIFEST = DOS_DIR / "install_manifest.json"
FLOPPY = DOS_DIR / FLOPPY_IMAGE
HD = DOS_DIR / HD_IMAGE
COMMAND = DOS_DIR / "COMMAND.COM"
STAMP = DOS_DIR / ".install_stamp"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_py(script: str, *args: str) -> None:
    cmd = [sys.executable, str(SCRIPTS / script), *args]
    if script.startswith("rtx_dos/"):
        cmd = [sys.executable, str(ROOT / "scripts" / script), *args]
    print(f"  run {' '.join(cmd)}")
    subprocess.run(cmd, check=True, cwd=ROOT)


def ensure_msdos_base() -> None:
    if MSDOS_BASE.is_dir() and (MSDOS_BASE / "v4.0-ozzie").is_dir():
        return
    MSDOS_BASE.parent.mkdir(parents=True, exist_ok=True)
    print("  clone Microsoft MS-DOS MIT source", flush=True)
    subprocess.run(
        ["git", "clone", "--depth", "1", "https://github.com/microsoft/MS-DOS.git", str(MSDOS_BASE)],
        check=True,
        cwd=ROOT,
    )


def manifest_ok() -> bool:
    if not MANIFEST.is_file():
        return False
    try:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return False
    return data.get("version") == MANIFEST_VERSION


def installed_ok() -> bool:
    return (
        manifest_ok()
        and FLOPPY.is_file()
        and HD.is_file()
        and COMMAND.is_file()
        and (AMMO_DIR / "IO.SYS").is_file()
        and (AMMO_DIR / "MSDOS.SYS").is_file()
        and (AMMO_DIR / "HIMEM.SYS").is_file()
        and (AMMO_DIR / "RTXKERNEL.SYS").is_file()
        and (AMMO_DIR / "RTXDRV.SYS").is_file()
        and (AMMO_DIR / "RTXHOST.SYS").is_file()
        and (AMMO_DIR / "RTXCD.SYS").is_file()
        and (AMMO_DIR / "AMMOFAT.SYS").is_file()
        and (AMMO_DIR / "FIELDLAY.TXT").is_file()
        and STAMP.is_file()
        and FLOPPY.stat().st_size == FLOPPY_BYTES
        and HD.stat().st_size == HD_BYTES
    )


def write_manifest() -> None:
    data = {
        "version": MANIFEST_VERSION,
        "guest_ram_gb": REPORTED_RAM_GB,
        "guest_ram_fast_mb": GUEST_RAM_FAST_MB,
        "hd_gb": HD_SIZE_GB,
        "hd_gpu_mirror_mb": 32,
        "os": PRODUCT_NAME,
        "tagline": TAGLINE,
        "lineage": LINEAGE,
        "boot": "Microsoft MS-DOS MIT IO.SYS/MSDOS.SYS (no FreeDOS)",
        "architecture": "GPU-primary Field Die; HD image backup on startup",
        "features": [
            "4 GB BIOS RAM + 64 MB GPU Field Die fast path",
            "4 GB logical C: RTXDOS — RAID spill + HD persistence",
            "MS-DOS 3.x–6.x + DPMI/XMS/DOS4GW program compatible",
            "MEMORYUP + SCANDISK + HKRTX registry + EXTMAP (2026)",
            "Field Commander mouse + association launcher",
            "RTXKERNEL Supercore INT 2Fh field mux L0-L9",
            "Field drive rack A/C/D/E (RTXDRV RTXHOST AMMOFAT RTXCD)",
            "Vulkan compute x86 + VGA (Super DOSBox class)",
            "INT15 E820 + A20 + XMS",
            "CONFIG.SYS menu RTX-DOS / Win31 / Win95 / Win98",
            "Home key DOS panel fullscreen toggle",
        ],
        "files": {
            FLOPPY_IMAGE: {"bytes": FLOPPY.stat().st_size, "sha256": sha256(FLOPPY)},
            HD_IMAGE: {"bytes": HD.stat().st_size, "sha256": sha256(HD)},
            "ammo/IO.SYS": {
                "bytes": (AMMO_DIR / "IO.SYS").stat().st_size,
                "sha256": sha256(AMMO_DIR / "IO.SYS"),
            },
            "ammo/MSDOS.SYS": {
                "bytes": (AMMO_DIR / "MSDOS.SYS").stat().st_size,
                "sha256": sha256(AMMO_DIR / "MSDOS.SYS"),
            },
            "COMMAND.COM": {"bytes": COMMAND.stat().st_size, "sha256": sha256(COMMAND)},
        },
    }
    MANIFEST.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    STAMP.write_text(data["files"][FLOPPY_IMAGE]["sha256"] + "\n", encoding="utf-8")


def install(force: bool = False) -> int:
    if installed_ok() and not force:
        print(f"{PRODUCT_NAME} install up to date ({MANIFEST})")
        return 0

    DOS_DIR.mkdir(parents=True, exist_ok=True)
    ensure_msdos_base()

    print(f"  patch Microsoft MS-DOS 4.00 MIT → {PRODUCT_NAME}", flush=True)
    run_py("rtx_dos/patch_msdos63.py")
    run_py("mk_tools_com.py")
    run_py("mk_dos_utils_com.py")
    run_py("gen_extmap.py")
    run_py("gen_rtxreg.py")

    print("  build RTX-DOS boot floppy (IO.SYS + MSDOS.SYS + CONFIG.SYS)", flush=True)
    run_py("mk_ammo_floppy.py", str(FLOPPY))

    print(f"  build {HD_SIZE_GB} GiB RTX-DOS C: (RTXDOS)", flush=True)
    run_py("mk_ammo_hd.py", str(HD))
    run_py("ammo_fdisk.py", str(HD))
    run_py("ammo_format.py", str(HD))

    shutil.copy2(AMMO_DIR / "COMMAND.COM", COMMAND)
    hd_dst = DOS_DIR / HD_IMAGE
    floppy_dst = DOS_DIR / FLOPPY_IMAGE
    if HD.resolve() != hd_dst.resolve():
        shutil.copy2(HD, hd_dst)
    if FLOPPY.resolve() != floppy_dst.resolve():
        shutil.copy2(FLOPPY, floppy_dst)

    run_py("install_rtx_windows.py")

    write_manifest()

    print()
    print(f"Installed {PRODUCT_NAME} ({TAGLINE}):")
    print(f"  {FLOPPY} ({FLOPPY.stat().st_size} bytes)")
    print(f"  {HD} ({HD.stat().st_size} bytes)")
    print(f"  {AMMO_DIR}/IO.SYS")
    print(f"  {AMMO_DIR}/MSDOS.SYS")
    print(f"  {COMMAND} ({COMMAND.stat().st_size} bytes)")
    print(f"  {MANIFEST}")
    return 0


def main() -> int:
    force = "--force" in sys.argv or "-f" in sys.argv
    return install(force=force)


if __name__ == "__main__":
    raise SystemExit(main())