#!/usr/bin/env python3
"""RTX-AMMOS Supercore — field-layer .SYS build orchestration (RTXFL + INT 2Fh mux).

Driver binaries are assembled from assets/dos/ammo/DRIVERS/*.ASM via AMMOASM (mk_drivers.py).
This module keeps RTXFL layout constants and legacy build_field_sys for QA comparisons.
mk_ammo_stubs.py imports this module; patch_msdos63.py runs mk_ammo_stubs before patching.
"""

from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

# Field-layer IDs — FieldLayer::LayerId (Navigator/engine/FieldLayerShell.hpp)
LAYER_RAM = 0
LAYER_VGA = 1
LAYER_FAT = 2
LAYER_DRIVES = 3
LAYER_VIEWPORT = 4
LAYER_AUDIO = 5
LAYER_MSCDEX = 6
LAYER_INPUT = 7
LAYER_IO = 8
LAYER_BIOS = 9

RTXFL_MAGIC = b"RTXFL"
LAYR_MAGIC = b"LAYR"
RTX_MUX_AH = 0x52
RTX_MUX_IDENT = 0x00
RTX_MUX_IOCTL = 0x01
RTX_MUX_ACTIV = 0x02

# RTXFL header offsets (shared with SUPERCORE.INC)
OFF_DEV = 3
OFF_SIG = 11
OFF_RTXFL = 19
OFF_LAYER = 24
OFF_IOCTL = 25
OFF_VERSION = 27
OFF_LAYR = 29
OFF_STRT = 32
OFF_INTR = 36
OFF_PARAS = 40

INIT_OFF = 48
BANNER_OFF = 64


@dataclass(frozen=True)
class FieldDevice:
    file_name: str
    dev_name: str
    signature: str
    layer_id: int
    ioctl_base: int
    version: int
    banner: str
    size: int = 160


def _emit_puts(banner_off: int) -> bytes:
    return bytes([
        0xB4, 0x09,
        0xBA, banner_off & 0xFF, (banner_off >> 8) & 0xFF,
        0xCD, 0x21,
    ])


def _emit_rtx_ident() -> bytes:
    return bytes([
        0xB8, RTX_MUX_IDENT, RTX_MUX_AH,
        0xCD, 0x2F,
    ])


def _emit_rtx_activ() -> bytes:
    return bytes([
        0xB8, 0x02, RTX_MUX_AH,
        0xCD, 0x2F,
    ])


def _emit_ioctl_thunk(layer_id: int, ioctl_base: int, thunk_off: int) -> bytes:
    """Near RET thunk: INT 2Fh RTX_MUX_IOCTL with BL=layer, CX=ioctl."""
    return bytes([
        0xB8, RTX_MUX_IOCTL & 0xFF, RTX_MUX_AH,
        0xB3, layer_id & 0xFF,
        0xB9, ioctl_base & 0xFF, (ioctl_base >> 8) & 0xFF,
        0xCD, 0x2F,
        0xC3,
    ])


def build_field_sys(dev: FieldDevice) -> bytes:
    stub = bytearray(dev.size)
    dev_b = dev.dev_name.encode("ascii")[:8].ljust(8, b"\x00")
    sig_b = dev.signature.encode("ascii")[:8].ljust(8, b"\x00")
    banner_b = dev.banner.encode("ascii") + b"\r\n$"

    if BANNER_OFF + len(banner_b) > dev.size:
        raise ValueError(f"{dev.file_name}: banner exceeds stub size {dev.size}")

    thunk_off = BANNER_OFF + ((len(banner_b) + 3) & ~3)
    if thunk_off + 12 > dev.size:
        raise ValueError(f"{dev.file_name}: no room for IOCTL thunk")

    stub[OFF_DEV:OFF_DEV + 8] = dev_b
    stub[OFF_SIG:OFF_SIG + 8] = sig_b
    stub[OFF_RTXFL:OFF_RTXFL + 5] = RTXFL_MAGIC
    stub[OFF_LAYER] = dev.layer_id & 0xFF
    struct.pack_into("<H", stub, OFF_IOCTL, dev.ioctl_base & 0xFFFF)
    struct.pack_into("<H", stub, OFF_VERSION, dev.version & 0xFFFF)
    stub[OFF_LAYR:OFF_LAYR + 4] = LAYR_MAGIC
    stub[OFF_STRT:OFF_STRT + 4] = b"STRT"
    stub[OFF_INTR:OFF_INTR + 4] = b"INTR"
    struct.pack_into("<H", stub, OFF_PARAS, dev.size // 16)

    stub[0] = 0xEB
    stub[1] = INIT_OFF - 2
    stub[2] = 0x90

    init = _emit_puts(BANNER_OFF) + _emit_rtx_ident() + bytes([0xC3])
    stub[INIT_OFF:INIT_OFF + len(init)] = init
    stub[BANNER_OFF:BANNER_OFF + len(banner_b)] = banner_b
    stub[thunk_off:thunk_off + 12] = _emit_ioctl_thunk(dev.layer_id, dev.ioctl_base, thunk_off)
    return bytes(stub)


def build_rtxkernel_sys() -> bytes:
    """256-byte Supercore kernel stub — banner + mux identify + layer activate."""
    size = 256
    dev = FieldDevice(
        "RTXKERNEL.SYS",
        "RTXKNL",
        "SUPRCR01",
        LAYER_RAM,
        0x2F50,
        0x0100,
        "RTXKERNEL Supercore v1.0 - INT 2Fh field mux L0-L9",
        size=size,
    )
    stub = bytearray(build_field_sys(dev))

    # Replace init: puts + ident + activ + ret
    init_off = INIT_OFF
    init = _emit_puts(BANNER_OFF) + _emit_rtx_ident() + _emit_rtx_activ() + bytes([0xC3])
    stub[init_off:init_off + len(init)] = init.ljust(len(stub[init_off:init_off + 16]), b"\x90")
    return bytes(stub)


# Layer L0 — XMS + floppy
HIMEM_DEV = FieldDevice(
    "HIMEM.SYS", "HIMEM", "HIMEM630", LAYER_RAM, 0x2F30, 0x0630,
    "HIMEM.SYS v6.30 XMS L0 loaded",
)
RTXDRV_DEV = FieldDevice(
    "RTXDRV.SYS", "RTXDRV", "RTXDRV01", LAYER_RAM, 0x2F05, 0x0100,
    "RTXDRV field layer L0 - floppy A: RTXBOOT loaded",
)
RTXKERNEL_DEV = None  # built separately

AMMOFAT_DEV = FieldDevice(
    "AMMOFAT.SYS", "AMMOFAT", "AMMOFAT1", LAYER_FAT, 0x2F10, 0x0100,
    "AMMOFAT v1.0 field layer L2 - GPU FAT16+ loaded",
)
RTXCD_DEV = FieldDevice(
    "RTXCD.SYS", "RTXCD", "RTXCD001", LAYER_MSCDEX, 0x1500, 0x0201,
    "RTXCD/MSCDEX 2.1 field layer L6 - driver RTXCD001 loaded",
)
RTXHOST_DEV = FieldDevice(
    "RTXHOST.SYS", "RTXHOST", "RTXHOST1", LAYER_IO, 0x2F40, 0x0100,
    "RTXHOST field layer L7 - host bridge E: loaded",
)
RTXSB_DEV = FieldDevice(
    "RTXSB.SYS", "RTXSB", "RTXSB001", LAYER_AUDIO, 0x2200, 0x0100,
    "RTXSB field layer L5 - SB16/OPL/GUS audio rack loaded",
)
RTXVGA_DEV = FieldDevice(
    "RTXVGA.SYS", "RTXVGA", "RTXVGA1", LAYER_VGA, 0x2F00, 0x0100,
    "RTXVGA field layer L1 - VGA/VESA text and graphics loaded",
)

EMM386_EXE = bytes([
    0xEB, 0x0C, 0x90,
    0x45, 0x4D, 0x4D, 0x33, 0x38, 0x36, 0x20, 0x36, 0x2E, 0x33, 0x00,
    0xB4, 0x09, 0xBA, 0x0F, 0x01, 0xCD, 0x21, 0xC3,
    0x45, 0x4D, 0x4D, 0x33, 0x38, 0x36, 0x20, 0x4F, 0x4B, 0x0D, 0x0A, 0x24,
])

SMARTDRV_EXE = bytes([
    0xEB, 0x0E, 0x90,
    0x53, 0x4D, 0x41, 0x52, 0x54, 0x44, 0x52, 0x56, 0x00,
    0xB8, 0x00, 0x4C, 0xCD, 0x21,
])

FIELD_DEVICES = [
    HIMEM_DEV, RTXDRV_DEV, RTXVGA_DEV, RTXSB_DEV,
    AMMOFAT_DEV, RTXCD_DEV, RTXHOST_DEV,
]


def verify_stub(path: Path, signature: bytes) -> None:
    data = path.read_bytes()
    if signature not in data:
        raise SystemExit(f"{path.name}: missing signature {signature!r}")
    if RTXFL_MAGIC not in data:
        raise SystemExit(f"{path.name}: missing {RTXFL_MAGIC!r} field-layer header")


def copy_ammoinc(out: Path) -> None:
    inc_src = Path(__file__).resolve().parent / "asm" / "SUPERCORE.INC"
    inc_dst = out / "AMMOINC" / "SUPERCORE.INC"
    inc_dst.parent.mkdir(parents=True, exist_ok=True)
    inc_dst.write_bytes(inc_src.read_bytes())
    rtx_inc = out / "AMMOINC" / "RTX.INC"
    if rtx_inc.is_file():
        text = rtx_inc.read_text(encoding="ascii")
        if "SUPERCORE.INC" not in text:
            rtx_inc.write_text(
                text.rstrip() + "\nINCLUDE SUPERCORE.INC\n",
                encoding="ascii",
            )


def emit_all(out: Path) -> dict[str, bytes]:
    out.mkdir(parents=True, exist_ok=True)
    artifacts: dict[str, bytes] = {}

    mk_drv = Path(__file__).resolve().parent / "mk_drivers.py"
    if mk_drv.is_file():
        subprocess.run([sys.executable, str(mk_drv), str(out)], check=True)
    else:
        for dev in FIELD_DEVICES:
            blob = build_field_sys(dev)
            (out / dev.file_name).write_bytes(blob)
            artifacts[dev.file_name] = blob
        kernel = build_rtxkernel_sys()
        (out / "RTXKERNEL.SYS").write_bytes(kernel)
        artifacts["RTXKERNEL.SYS"] = kernel

    for name in (
        "RTXKERNEL.SYS", "HIMEM.SYS", "RTXDRV.SYS", "RTXVGA.SYS", "RTXSB.SYS",
        "AMMOFAT.SYS", "RTXCD.SYS", "RTXHOST.SYS",
    ):
        p = out / name
        if p.is_file():
            artifacts[name] = p.read_bytes()

    (out / "EMM386.EXE").write_bytes(EMM386_EXE)
    (out / "SMARTDRV.EXE").write_bytes(SMARTDRV_EXE)
    artifacts["EMM386.EXE"] = EMM386_EXE
    artifacts["SMARTDRV.EXE"] = SMARTDRV_EXE

    verify_stub(out / "AMMOFAT.SYS", b"AMMOFAT1")
    verify_stub(out / "RTXCD.SYS", b"RTXCD001")
    verify_stub(out / "RTXDRV.SYS", b"RTXDRV01")
    verify_stub(out / "RTXHOST.SYS", b"RTXHOST1")
    verify_stub(out / "RTXKERNEL.SYS", b"SUPRCR01")
    verify_stub(out / "RTXSB.SYS", b"RTXSB001")
    verify_stub(out / "RTXVGA.SYS", b"RTXVGA1")

    copy_ammoinc(out)
    drivers_src = Path(__file__).resolve().parents[2] / "assets" / "dos" / "ammo" / "DRIVERS"
    drivers_dst = out / "DRIVERS"
    if drivers_src.is_dir() and drivers_src.resolve() != drivers_dst.resolve():
        drivers_dst.mkdir(parents=True, exist_ok=True)
        for asm in drivers_src.glob("*.ASM"):
            shutil.copy2(asm, drivers_dst / asm.name)
        for inc in drivers_src.glob("*.INC"):
            shutil.copy2(inc, drivers_dst / inc.name)
    return artifacts


def main() -> int:
    import sys

    root = Path(__file__).resolve().parents[3]
    out = Path(sys.argv[1] if len(sys.argv) > 1 else root / "assets" / "dos" / "ammo")
    emit_all(out)

    import subprocess

    devkit = Path(__file__).parent / "mk_ammo_devkit.py"
    if devkit.is_file():
        subprocess.run([sys.executable, str(devkit), str(out)], check=True)

    print(
        f"supercore: AMMOASM-built RTXFL drivers "
        f"(RTXKERNEL RTXDRV AMMOFAT RTXCD RTXHOST HIMEM RTXSB RTXVGA) → {out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())