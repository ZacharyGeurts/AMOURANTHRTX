#!/usr/bin/env python3
"""Download doom19s.zip and extract Shareware DOOM.EXE + DOOM1.WAD (id v1.9)."""
from __future__ import annotations

import io
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
DOS_DIR = ROOT / "assets" / "dos"
WAD_DIR = ROOT / "assets" / "wads"
OUT_EXE = DOS_DIR / "DOOM.EXE"
OUT_WAD = WAD_DIR / "doom1.wad"
URLS = (
    "https://ftpmirror1.infania.net/pub/idgames/idstuff/doom/doom19s.zip",
    "https://www.gamers.org/pub/idgames/idstuff/doom/doom19s.zip",
)


def download(url: str, dest: Path) -> None:
    print(f"  fetch {url}", flush=True)
    req = Request(url, headers={"User-Agent": "AMOURANTHRTX/1.0"})
    with urlopen(req, timeout=120) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out)


def extract_shareware(archive: Path) -> dict[str, bytes]:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        with zipfile.ZipFile(archive) as outer:
            outer.extractall(root)
        part1 = root / "DOOMS_19.1"
        part2 = root / "DOOMS_19.2"
        if not part1.is_file() or not part2.is_file():
            raise SystemExit("doom19s.zip missing DOOMS_19.1 / DOOMS_19.2 split archives")
        blob = part1.read_bytes() + part2.read_bytes()
        for start in range(0, 4096):
            if blob[start : start + 2] != b"PK":
                continue
            try:
                inner = zipfile.ZipFile(io.BytesIO(blob[start:]))
            except zipfile.BadZipFile:
                continue
            out: dict[str, bytes] = {}
            for name in inner.namelist():
                upper = name.upper()
                if upper in ("DOOM.EXE", "DOOM1.WAD"):
                    data = inner.read(name)
                    out[upper] = data
            if "DOOM.EXE" in out and "DOOM1.WAD" in out:
                if out["DOOM1.WAD"][:4] != b"IWAD":
                    raise SystemExit("DOOM1.WAD is not an IWAD")
                if out["DOOM.EXE"][:2] != b"MZ":
                    raise SystemExit("DOOM.EXE missing MZ signature")
                return out
        raise SystemExit("DOOM.EXE / DOOM1.WAD not found inside doom19s split archive")


def install(force: bool = False) -> int:
    if OUT_EXE.is_file() and OUT_WAD.is_file() and not force:
        print(f"Shareware Doom ready: {OUT_EXE.name} ({OUT_EXE.stat().st_size} bytes)")
        print(f"                      {OUT_WAD} ({OUT_WAD.stat().st_size} bytes)")
        return 0

    DOS_DIR.mkdir(parents=True, exist_ok=True)
    WAD_DIR.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        zpath = Path(tmp) / "doom19s.zip"
        last_err: Exception | None = None
        for url in URLS:
            try:
                download(url, zpath)
                last_err = None
                break
            except Exception as exc:  # noqa: BLE001
                last_err = exc
        if last_err is not None:
            raise SystemExit(f"Failed to download doom19s.zip: {last_err}")

        files = extract_shareware(zpath)
        OUT_EXE.write_bytes(files["DOOM.EXE"])
        OUT_WAD.write_bytes(files["DOOM1.WAD"])
        (DOS_DIR / "DOOM1.WAD").write_bytes(files["DOOM1.WAD"])

    print(f"Installed: {OUT_EXE} ({OUT_EXE.stat().st_size} bytes)")
    print(f"Installed: {OUT_WAD} ({OUT_WAD.stat().st_size} bytes)")
    dos_wad = DOS_DIR / "DOOM1.WAD"
    if dos_wad.is_file():
        print(f"Installed: {dos_wad} ({dos_wad.stat().st_size} bytes)")
    return 0


def main() -> int:
    force = "--force" in sys.argv or "-f" in sys.argv
    return install(force=force)


if __name__ == "__main__":
    raise SystemExit(main())