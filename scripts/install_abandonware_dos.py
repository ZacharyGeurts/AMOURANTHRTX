#!/usr/bin/env python3
"""Fetch legal shareware DOS abandonware ZIPs into assets/dos/games/ for AMMOZIP on RTX-DOS."""
from __future__ import annotations

import json
import shutil
import sys
import tempfile
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
GAMES_DIR = ROOT / "assets" / "dos" / "games"
MANIFEST = GAMES_DIR / "games_manifest.json"

USER_AGENT = "AMOURANTHRTX/1.0 (shareware fetcher)"


def download(url: str, dest: Path) -> None:
    print(f"  fetch {url}", flush=True)
    req = Request(url, headers={"User-Agent": USER_AGENT})
    with urlopen(req, timeout=180) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out)


def stage_zip(id_: str, title: str, zip_name: str, src: Path) -> dict:
    dest = GAMES_DIR / id_
    if dest.is_dir():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    staged_name = zip_name.upper()
    (dest / staged_name).write_bytes(src.read_bytes())
    entry = {
        "id": id_,
        "title": title,
        "zip": staged_name,
        "exe": None,
        "dos_path": f"C:\\GAMES\\{id_.upper()}\\{staged_name}",
        "files": [staged_name],
        "extract_hint": f"AMMOZIP C:\\GAMES\\{id_.upper()}\\{staged_name}",
    }
    print(f"  staged {title}: {staged_name} ({src.stat().st_size} bytes)")
    return entry


def install_wolf3d(force: bool) -> dict | None:
    dest = GAMES_DIR / "wolf3d"
    zip_on_disk = dest / "WOLF3D.ZIP"
    if not force and zip_on_disk.is_file():
        return entry_from_disk("wolf3d", "Wolfenstein 3D", "WOLF3D.ZIP")
    urls = ("https://archive.org/download/wolf3dsw/wolf3dsw.zip",)
    with tempfile.TemporaryDirectory() as tmp:
        zpath = Path(tmp) / "wolf3dsw.zip"
        ok = False
        for url in urls:
            try:
                download(url, zpath)
                ok = True
                break
            except Exception as exc:  # noqa: BLE001
                print(f"  warn wolf3d: {exc}")
        if not ok:
            return None
        return stage_zip("wolf3d", "Wolfenstein 3D", "WOLF3D.ZIP", zpath)


def install_cosmo(force: bool) -> dict | None:
    dest = GAMES_DIR / "cosmo"
    zip_on_disk = dest / "COSMO.ZIP"
    if not force and zip_on_disk.is_file():
        return entry_from_disk("cosmo", "Cosmo's Cosmic Adventure", "COSMO.ZIP")
    urls = (
        "https://archive.org/download/cosmo_202201/Cosmo.zip",
        "https://www.dosgamesarchive.com/file/cosmos-cosmic-adventure/1cosmo",
    )
    with tempfile.TemporaryDirectory() as tmp:
        zpath = Path(tmp) / "cosmo.zip"
        ok = False
        for url in urls:
            try:
                download(url, zpath)
                ok = True
                break
            except Exception as exc:  # noqa: BLE001
                print(f"  warn cosmo: {exc}")
        if not ok:
            return None
        return stage_zip("cosmo", "Cosmo's Cosmic Adventure", "COSMO.ZIP", zpath)


def install_keen4_from_disk() -> dict | None:
    dest = GAMES_DIR / "keen4"
    exe = dest / "KEEN4E.EXE"
    if not exe.is_file():
        return None
    files = sorted(p.name for p in dest.iterdir() if p.is_file())
    return {
        "id": "keen4",
        "title": "Commander Keen 4 (shareware)",
        "zip": None,
        "exe": "KEEN4E.EXE",
        "dos_path": "C:\\GAMES\\KEEN4\\KEEN4E.EXE",
        "files": files,
        "extract_hint": "Run KEEN4E.EXE from C:\\GAMES\\KEEN4",
    }


def install_doom_from_disk() -> dict | None:
    dest = GAMES_DIR / "doom"
    exe = dest / "DOOM.EXE"
    wad = dest / "DOOM1.WAD"
    if not exe.is_file():
        return None
    files = ["DOOM.EXE"]
    if wad.is_file():
        files.append("DOOM1.WAD")
    return {
        "id": "doom",
        "title": "Doom (shareware)",
        "zip": None,
        "exe": "DOOM.EXE",
        "dos_path": "C:\\GAMES\\DOOM\\DOOM.EXE",
        "files": files,
        "extract_hint": "Run DOOM.EXE from C:\\GAMES\\DOOM",
    }


def entry_from_disk(id_: str, title: str, zip_name: str) -> dict | None:
    dest = GAMES_DIR / id_
    zf = dest / zip_name.upper()
    if not zf.is_file():
        return None
    return {
        "id": id_,
        "title": title,
        "zip": zip_name.upper(),
        "exe": None,
        "dos_path": f"C:\\GAMES\\{id_.upper()}\\{zip_name.upper()}",
        "files": [zip_name.upper()],
        "extract_hint": f"AMMOZIP C:\\GAMES\\{id_.upper()}\\{zip_name.upper()}",
    }


def install(force: bool = False) -> int:
    GAMES_DIR.mkdir(parents=True, exist_ok=True)
    entries: list[dict] = []
    catalog = (
        ("wolf3d", "Wolfenstein 3D", "WOLF3D.ZIP", install_wolf3d),
        ("cosmo", "Cosmo's Cosmic Adventure", "COSMO.ZIP", install_cosmo),
    )
    for id_, title, zip_name, fn in catalog:
        try:
            ent = fn(force)
            if not ent:
                ent = entry_from_disk(id_, title, zip_name)
            if ent:
                entries.append(ent)
        except Exception as exc:  # noqa: BLE001
            print(f"  FAIL {fn.__name__}: {exc}")

    keen = install_keen4_from_disk()
    if keen:
        entries.append(keen)
        print(f"  staged Keen4: KEEN4E.EXE ({(GAMES_DIR / 'keen4' / 'KEEN4E.EXE').stat().st_size} bytes)")

    doom = install_doom_from_disk()
    if doom:
        entries.append(doom)
        print(f"  staged Doom: DOOM.EXE ({(GAMES_DIR / 'doom' / 'DOOM.EXE').stat().st_size} bytes)")

    if not entries:
        print("No game ZIPs installed — check network mirrors")
        return 1

    manifest = {"version": 2, "games": entries}
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {MANIFEST} ({len(entries)} game archives — extract with AMMOZIP on C:)")
    return 0


def main() -> int:
    force = "--force" in sys.argv or "-f" in sys.argv
    return install(force=force)


if __name__ == "__main__":
    raise SystemExit(main())