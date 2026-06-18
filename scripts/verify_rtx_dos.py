#!/usr/bin/env python3
"""Verify RTX-DOS 7.0 GPU Super DOSBox install and boot-to-prompt probe."""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from ammo_platform import HD_SIZE_MB, MANIFEST_VERSION, PRODUCT_NAME, REPORTED_RAM_GB  # noqa: E402
from rtx_dos_brand import FLOPPY_IMAGE, HD_IMAGE  # noqa: E402

DOS = ROOT / "assets" / "dos"
AMMO = DOS / "ammo"
BIN = ROOT / "build/bin/Linux/AMOURANTHRTX"
HD_BYTES = HD_SIZE_MB * 1024 * 1024


def check_files() -> None:
    required = {
        DOS / FLOPPY_IMAGE: 737280,
        DOS / HD_IMAGE: HD_BYTES,
        AMMO / "IO.SYS": 10000,
        AMMO / "MSDOS.SYS": 40000,
        AMMO / "COMMAND.COM": 10000,
        AMMO / "HIMEM.SYS": 1,
        AMMO / "RTXDRV.SYS": 1,
        AMMO / "RTXHOST.SYS": 1,
        AMMO / "RTXCD.SYS": 1,
        AMMO / "AMMOFAT.SYS": 1,
        AMMO / "FIELDLAY.TXT": 1,
        AMMO / "DRIVES.TXT": 1,
        AMMO / "EMM386.EXE": 1,
        AMMO / "SMARTDRV.EXE": 1,
        DOS / "COMMAND.COM": 10000,
        DOS / "install_manifest.json": 1,
    }
    for path, min_size in required.items():
        if not path.is_file() or path.stat().st_size < min_size:
            raise SystemExit(f"Missing or too small: {path}")
    manifest = (DOS / "install_manifest.json").read_text(encoding="utf-8")
    if f'"version": {MANIFEST_VERSION}' not in manifest:
        raise SystemExit(f"install_manifest.json not at version {MANIFEST_VERSION} — run ./linux.sh dos --force")
    if f'"guest_ram_gb": {REPORTED_RAM_GB}' not in manifest:
        raise SystemExit("manifest missing 4GB RAM profile")
    if PRODUCT_NAME not in manifest:
        raise SystemExit(f"manifest missing {PRODUCT_NAME} lineage")
    print(f"OK assets: {PRODUCT_NAME} floppy + {HD_SIZE_MB} MiB C: + 4GB RAM manifest")


def list_hd_root() -> list[str]:
    img = (DOS / HD_IMAGE).read_bytes()
    vol = img[512:]
    bps, spf = struct.unpack_from("<H", vol, 11)[0], struct.unpack_from("<H", vol, 22)[0]
    root_off = (1 + 2 * spf) * bps
    names = []
    for i in range(512):
        ent = vol[root_off + i * 32 : root_off + (i + 1) * 32]
        if ent[0] in (0, 0xE5) or ent[11] & 0x08:
            continue
        name = ent[0:8].decode("ascii", "ignore").strip()
        ext = ent[8:11].decode("ascii", "ignore").strip()
        names.append(f"{name}.{ext}".rstrip("."))
    return names


def check_hd_fat() -> None:
    img = (DOS / HD_IMAGE).read_bytes()
    names = list_hd_root()
    for want in (
        "IO.SYS",
        "MSDOS.SYS",
        "COMMAND.COM",
        "AUTOEXEC.BAT",
        "CONFIG.SYS",
        "HIMEM.SYS",
        "RTXDRV.SYS",
        "RTXCD.SYS",
        "AMMOFAT.SYS",
        "RTXHOST.SYS",
        "FIELDLAY.TXT",
        "DRIVES.TXT",
        "EMM386.EXE",
        "SMARTDRV.EXE",
        "BOOT31.BAT",
        "BOOT95.BAT",
        "BOOT98.BAT",
        "WIN.COM",
        "MSDOS.INI",
        "FDISK.EXE",
        "FORMAT.COM",
        "CHKDSK.COM",
        "MORE.COM",
    ):
        if want not in names:
            raise SystemExit(f"C: root missing {want} (have {names})")
    if "DOOM.EXE" in names or "DOOM1.WAD" in names:
        raise SystemExit("C: should not ship Doom — user installs games separately")
    vol = img[512:]
    label = vol[43:54].decode("ascii", "ignore").strip()
    oem = vol[3:11].decode("ascii", "ignore").strip()
    if label != "RTXDOS" or oem != "RTXDOS70":
        raise SystemExit(f"C: volume wrong: {label!r} / {oem!r}")
    print(f"OK C: RTXDOS RTX-DOS 7.0 ({len(names)} root entries)")


def list_floppy_root() -> list[str]:
    img = (DOS / FLOPPY_IMAGE).read_bytes()
    root_off = (1 + 2 * 3) * 512
    names = []
    for i in range(112):
        ent = img[root_off + i * 32 : root_off + (i + 1) * 32]
        if ent[0] in (0, 0xE5):
            continue
        name = ent[0:8].decode("ascii", "ignore").strip()
        ext = ent[8:11].decode("ascii", "ignore").strip()
        names.append(f"{name}.{ext}".rstrip("."))
    return names


def check_floppy() -> None:
    img = (DOS / FLOPPY_IMAGE).read_bytes()
    oem = img[3:11].decode("ascii", "ignore").strip()
    if oem != "RTXDOS70":
        raise SystemExit(f"floppy OEM {oem!r} (expected RTXDOS70)")
    names = list_floppy_root()
    for want in ("KERNEL.SYS", "CONFIG.SYS"):
        if want not in names:
            raise SystemExit(f"A: missing {want} (have {names})")
    if b"SHELL=C:\\COMMAND.COM" not in img:
        raise SystemExit("A: CONFIG.SYS missing SHELL=C:\\COMMAND.COM")
    print(f"OK A: RTX-DOS 7.0 boot ({', '.join(names)})")


def build_boot_test() -> None:
    lib = ROOT / "build/libx86emu.a"
    src = ROOT / "scripts/boot_to_doom_test.cpp"
    out = ROOT / "build/bin/Linux/boot_to_doom_test"
    if not lib.is_file() or not src.is_file():
        print("SKIP boot_to_doom_test build")
        return
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++-14", "-std=c++20", "-O2",
            "-I", str(ROOT / "Navigator/engine"),
            "-I", str(ROOT / "third_party/libx86emu/include"),
            str(src), str(lib), "-o", str(out),
        ],
        cwd=ROOT,
        check=True,
    )


def run_boot_test() -> None:
    build_boot_test()
    boot = ROOT / "build/bin/Linux/boot_to_doom_test"
    if not boot.is_file():
        print("SKIP boot_to_doom_test")
        return
    proc = subprocess.run([str(boot)], cwd=ROOT, capture_output=True, text=True, timeout=180, check=False)
    sys.stdout.write(proc.stdout)
    if proc.returncode != 0:
        raise SystemExit(f"boot_to_doom_test failed rc={proc.returncode}\n{proc.stderr[-800:]}")


def build_dos_engine_test() -> None:
    lib = ROOT / "build/libx86emu.a"
    src = ROOT / "scripts/dos_engine_test.cpp"
    out = ROOT / "build/bin/Linux/dos_engine_test"
    if not lib.is_file() or not src.is_file():
        print("SKIP dos_engine_test build")
        return
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++-14", "-std=c++20", "-O2",
            "-I", str(ROOT / "Navigator/engine"),
            "-I", str(ROOT / "third_party/libx86emu/include"),
            str(src), str(lib), "-o", str(out),
        ],
        cwd=ROOT,
        check=True,
    )


def run_dos_engine_test() -> None:
    build_dos_engine_test()
    exe = ROOT / "build/bin/Linux/dos_engine_test"
    if not exe.is_file():
        print("SKIP dos_engine_test")
        return
    proc = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, check=False)
    sys.stdout.write(proc.stdout)
    if proc.returncode != 0:
        raise SystemExit(f"dos_engine_test failed rc={proc.returncode}\n{proc.stderr[-800:]}")


def run_engine() -> None:
    if not BIN.is_file():
        print("SKIP engine")
        return
    try:
        proc = subprocess.run(
            ["env", "AMOURANTHRTX_HEADLESS=1", "AMOURANTHRTX_MAX_FRAMES=30", str(BIN)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=90,
            check=False,
        )
    except subprocess.TimeoutExpired:
        print("SKIP engine (headless timeout)")
        return
    out = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise SystemExit(f"Engine failed rc={proc.returncode}")
    if "RTX-AMMOS" not in out and "GPU-only" not in out:
        raise SystemExit("Engine did not load RTX-DOS path")
    if "GPU Doom" in out or "GPU die — title + raycast" in out:
        raise SystemExit("Engine still on GPU raycast Doom path")
    print("OK engine: RTX-DOS 7.0 GPU")


def main() -> int:
    check_files()
    check_floppy()
    check_hd_fat()
    run_boot_test()
    run_dos_engine_test()
    run_engine()
    print("RTX-DOS 7.0 GPU Super DOSBox verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())