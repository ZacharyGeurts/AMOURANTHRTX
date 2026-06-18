#!/usr/bin/env python3
"""Build FAT16 virtual C: (200 MiB) — MS-DOS 6.30 Codename Ammo system partition."""

from __future__ import annotations

import datetime
import struct
import sys
from pathlib import Path

SECTOR = 512
SECTORS_PER_CLUSTER = 16
CLUSTER_BYTES = SECTOR * SECTORS_PER_CLUSTER
try:
    from ammo_platform import HD_SIZE_MB
except ImportError:
    HD_SIZE_MB = 2048
TOTAL_SECTORS = (HD_SIZE_MB * 1024 * 1024) // SECTOR
PART_LBA = 1
PART_SECTORS = TOTAL_SECTORS - PART_LBA
RESERVED = 1
FATS = 2
ROOT_ENTRIES = 512
ROOT_SECTORS = (ROOT_ENTRIES * 32 + SECTOR - 1) // SECTOR
SPT = 63
HEADS = 16

ROOT = Path(__file__).resolve().parents[1]
AMMO = ROOT / "assets" / "dos" / "ammo"
BUILD = ROOT / "scripts" / "rtx_dos"
sys.path.insert(0, str(BUILD))
from bootsect_hd import emit as emit_bootsect  # noqa: E402

PRIORITY = [
    "IO.SYS",
    "MSDOS.SYS",
    "COMMAND.COM",
    "CONFIG.SYS",
    "AUTOEXEC.BAT",
    "RTXKERNEL.SYS",
    "HIMEM.SYS",
    "RTXDRV.SYS",
    "RTXVGA.SYS",
    "RTXSB.SYS",
    "RTXCD.SYS",
    "AMMOFAT.SYS",
    "RTXHOST.SYS",
    "FIELDLAY.TXT",
    "DRIVES.TXT",
    "AMMOFAT.TXT",
    "EMM386.EXE",
    "SMARTDRV.EXE",
    "FDISK.EXE",
    "FORMAT.COM",
    "SYS.COM",
    "CHKDSK.COM",
    "CHKDSK.EXE",
    "MEMORYUP.COM",
    "SCANDISK.COM",
    "REGEDIT.COM",
    "COUNTRY.SYS",
    "MORE.COM",
    "PRINT.COM",
    "DISKCOPY.COM",
    "DEBUG.COM",
    "EDLIN.COM",
    "RECOVER.COM",
    "VERSION.TXT",
    "README.TXT",
    "DESCRIPT.ION",
    "MSDOS.INI",
    "BOOT31.BAT",
    "BOOT95.BAT",
    "BOOT98.BAT",
    "WIN31.TXT",
    "COMMAND.TXT",
    "GOLDEN.TXT",
    "DESCRIPT.ION",
    "WIN.COM",
]

SUBDIRS = ("WINDOWS", "TOOLS", "DOS", "WIN", "AMMOINC", "SAMPLES", "SOUND", "QBASIC", "AMMOCODE", "GAMES")

SYS_ATTR = 0x27
BAT_ATTR = 0x20
DIR_ATTR = 0x10


def sectors_per_fat(total_sectors: int) -> int:
    data_area = total_sectors - RESERVED - ROOT_SECTORS
    spf = 1
    for _ in range(16):
        clusters = data_area - FATS * spf
        need = (clusters * 2 + SECTOR - 1) // SECTOR
        if need == spf:
            break
        spf = need
    return max(1, spf)


def lba_to_chs(lba: int) -> tuple[int, int, int]:
    cyl = lba // (SPT * HEADS)
    rem = lba % (SPT * HEADS)
    head = rem // SPT
    sector = (rem % SPT) + 1
    return cyl, head, sector


def fat_set(fat: bytearray, cluster: int, value: int) -> None:
    off = cluster * 2
    fat[off] = value & 0xFF
    fat[off + 1] = (value >> 8) & 0xFF


def fat_timestamp(dt: datetime.datetime | None = None) -> tuple[int, int]:
    if dt is None:
        dt = datetime.datetime.now()
    year = max(1980, min(2107, dt.year))
    date = ((year - 1980) << 9) | (dt.month << 5) | dt.day
    time = (dt.hour << 11) | (dt.minute << 5) | (dt.second // 2)
    return time, date


def write_dir_entry(name: str, size: int, cluster: int, attr: int = 0x20) -> bytes:
    stem, _, ext = name.upper().partition(".")
    ent = bytearray(32)
    ent[0:8] = f"{stem:<8}"[:8].encode("ascii")
    ent[8:11] = f"{ext:<3}"[:3].encode("ascii")
    ent[11] = attr
    fat_time, fat_date = fat_timestamp()
    struct.pack_into("<H", ent, 22, fat_time)
    struct.pack_into("<H", ent, 24, fat_date)
    struct.pack_into("<H", ent, 26, cluster)
    struct.pack_into("<I", ent, 28, size)
    return bytes(ent)


def write_dot_entry(dot: str, cluster: int) -> bytes:
    ent = bytearray(32)
    if dot == ".":
        ent[0] = ord(".")
    else:
        ent[0] = ord(".")
        ent[1] = ord(".")
    ent[11] = DIR_ATTR
    fat_time, fat_date = fat_timestamp()
    struct.pack_into("<H", ent, 22, fat_time)
    struct.pack_into("<H", ent, 24, fat_date)
    struct.pack_into("<H", ent, 26, cluster)
    return bytes(ent)


def file_attr(name: str) -> int:
    upper = name.upper()
    if upper.endswith(".SYS"):
        return SYS_ATTR
    return BAT_ATTR


def build_subdir(
    vol: bytearray,
    fat: bytearray,
    next_clus: int,
    files: list[tuple[str, bytes, int]],
    parent_clus: int,
    data_start: int,
) -> tuple[int, int]:
    file_ents: list[bytes] = []
    for name, data, attr in files:
        clus = next_clus
        next_clus = alloc_chain(vol, fat, next_clus, data, data_start)
        file_ents.append(write_dir_entry(name, len(data), clus, attr))
    dir_clus = next_clus
    body = write_dot_entry(".", dir_clus) + write_dot_entry("..", parent_clus) + b"".join(file_ents)
    padded = body.ljust(
        max(CLUSTER_BYTES, ((len(body) + CLUSTER_BYTES - 1) // CLUSTER_BYTES) * CLUSTER_BYTES),
        b"\x00",
    )
    next_clus = alloc_chain(vol, fat, dir_clus, padded, data_start)
    return dir_clus, next_clus


def write_cluster(vol: bytearray, cluster: int, data: bytes, data_start: int) -> None:
    sector_off = data_start + (cluster - 2) * SECTORS_PER_CLUSTER
    byte_off = sector_off * SECTOR
    vol[byte_off:byte_off + CLUSTER_BYTES] = data[:CLUSTER_BYTES].ljust(CLUSTER_BYTES, b"\x00")


def alloc_chain(vol: bytearray, fat: bytearray, start_cluster: int, data: bytes, data_start: int) -> int:
    clusters_needed = max(1, (len(data) + CLUSTER_BYTES - 1) // CLUSTER_BYTES)
    c = start_cluster
    for i in range(clusters_needed):
        chunk = data[i * CLUSTER_BYTES:(i + 1) * CLUSTER_BYTES]
        write_cluster(vol, c, chunk, data_start)
        fat_set(fat, c, 0xFFFF if i == clusters_needed - 1 else c + 1)
        c += 1
    return c


def write_mbr(img: bytearray) -> None:
    img[0:2] = b"\xFA\x33"
    img[510:512] = b"\x55\xAA"
    cyl, head, sector = lba_to_chs(PART_LBA)
    end_lba = PART_LBA + PART_SECTORS - 1
    ecyl, ehead, esector = lba_to_chs(end_lba)
    ent = bytearray(16)
    ent[0] = 0x80
    ent[1] = head
    ent[2] = sector | ((cyl & 0x300) >> 2)
    ent[3] = cyl & 0xFF
    ent[4] = 0x06
    ent[5] = ehead
    ent[6] = esector | ((ecyl & 0x300) >> 2)
    ent[7] = ecyl & 0xFF
    struct.pack_into("<I", ent, 8, PART_LBA)
    struct.pack_into("<I", ent, 12, PART_SECTORS)
    img[0x1BE:0x1BE + 16] = ent


def collect_root_entries() -> list[tuple[str, bytes, int]]:
    if not (AMMO / "IO.SYS").is_file():
        raise SystemExit(f"Run scripts/rtx_dos/patch_msdos63.py first ({AMMO})")

    by_name: dict[str, tuple[bytes, int]] = {}
    for path in sorted(AMMO.iterdir()):
        if not path.is_file() or path.name == "ammo_manifest.json":
            continue
        name = path.name.upper()
        by_name[name] = (path.read_bytes(), file_attr(name))

    ordered: list[tuple[str, bytes, int]] = []
    seen: set[str] = set()
    for name in PRIORITY:
        if name in by_name:
            data, attr = by_name[name]
            ordered.append((name, data, attr))
            seen.add(name)
    for name in sorted(by_name):
        if name not in seen:
            data, attr = by_name[name]
            ordered.append((name, data, attr))
    return ordered


def collect_subdir_files(sub: str) -> list[tuple[str, bytes, int]]:
    base = AMMO / sub
    if not base.is_dir():
        return []
    out: list[tuple[str, bytes, int]] = []
    for path in sorted(base.iterdir()):
        if not path.is_file():
            continue
        name = path.name.upper()
        out.append((name, path.read_bytes(), file_attr(name)))
    return out


def subdir_has_children(sub: str) -> bool:
    base = AMMO / sub
    return base.is_dir() and any(p.is_dir() for p in base.iterdir())


def build_nested_subdir(
    vol: bytearray,
    fat: bytearray,
    next_clus: int,
    base: Path,
    parent_clus: int,
    data_start: int,
) -> tuple[int, int]:
    """Preserve nested folders (e.g. GAMES/DOOM/DOOM.EXE) on the FAT16 volume."""
    file_ents: list[bytes] = []
    sub_ents: list[bytes] = []
    for child in sorted(base.iterdir()):
        if child.is_file():
            data = child.read_bytes()
            name = child.name.upper()
            clus = next_clus
            next_clus = alloc_chain(vol, fat, next_clus, data, data_start)
            file_ents.append(write_dir_entry(name, len(data), clus, file_attr(name)))
        elif child.is_dir():
            sub_clus, next_clus = build_nested_subdir(
                vol, fat, next_clus, child, 0, data_start
            )
            sub_ents.append(write_dir_entry(child.name.upper(), 0, sub_clus, DIR_ATTR))

    dir_clus = next_clus
    body = write_dot_entry(".", dir_clus) + write_dot_entry("..", parent_clus)
    body += b"".join(sub_ents + file_ents)
    padded = body.ljust(
        max(CLUSTER_BYTES, ((len(body) + CLUSTER_BYTES - 1) // CLUSTER_BYTES) * CLUSTER_BYTES),
        b"\x00",
    )
    next_clus = alloc_chain(vol, fat, dir_clus, padded, data_start)

    dot_off = (data_start + (dir_clus - 2) * SECTORS_PER_CLUSTER) * SECTOR
    for ent in sub_ents:
        name = ent[0:8].decode("ascii").strip()
        sub_clus = struct.unpack_from("<H", ent, 26)[0]
        sub_off = (data_start + (sub_clus - 2) * SECTORS_PER_CLUSTER) * SECTOR + 32
        struct.pack_into("<H", vol, sub_off + 26, dir_clus)

    return dir_clus, next_clus


def main() -> int:
    from rtx_dos_brand import HD_IMAGE  # noqa: E402

    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "assets" / "dos" / HD_IMAGE)
    entries = collect_root_entries()

    img = bytearray(TOTAL_SECTORS * SECTOR)
    write_mbr(img)

    spf = sectors_per_fat(PART_SECTORS)
    data_start = RESERVED + FATS * spf + ROOT_SECTORS

    vol = bytearray(PART_SECTORS * SECTOR)
    vol[0:SECTOR] = emit_bootsect(PART_SECTORS)

    fat = bytearray(spf * SECTOR)
    fat_set(fat, 0, 0xFFF8)
    fat_set(fat, 1, 0xFFFF)

    next_clus = 2
    dir_entries: list[bytes] = []
    subdir_names: list[str] = []
    for sub in SUBDIRS:
        base = AMMO / sub
        if not base.is_dir():
            continue
        if subdir_has_children(sub):
            dir_clus, next_clus = build_nested_subdir(
                vol, fat, next_clus, base, 0, data_start
            )
        else:
            files = collect_subdir_files(sub)
            if not files:
                continue
            dir_clus, next_clus = build_subdir(vol, fat, next_clus, files, 0, data_start)
        dir_entries.append(write_dir_entry(sub, 0, dir_clus, DIR_ATTR))
        subdir_names.append(sub)

    for name, data, attr in entries:
        clus = next_clus
        next_clus = alloc_chain(vol, fat, next_clus, data, data_start)
        dir_entries.append(write_dir_entry(name, len(data), clus, attr))

    root_off = RESERVED * SECTOR + FATS * spf * SECTOR
    for i, ent in enumerate(dir_entries):
        vol[root_off + i * 32:root_off + (i + 1) * 32] = ent

    for f in range(FATS):
        off = (RESERVED + f) * SECTOR
        vol[off:off + len(fat)] = fat

    img[PART_LBA * SECTOR:PART_LBA * SECTOR + len(vol)] = vol

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img)
    names = ", ".join(n for n, _, _ in entries[:10])
    sub_note = f", dirs: {', '.join(subdir_names)}" if subdir_names else ""
    extra = len(entries) - 10
    suffix = f" +{extra} more" if extra > 0 else ""
    print(f"wrote {out} ({len(img)} bytes, RTX-DOS 7.0 C: RTXDOS, {names}{suffix}{sub_note})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())