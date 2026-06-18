#!/usr/bin/env python3
"""Stage assets/dos/games/* into ammo/GAMES/ and rebuild RTX-DOS HD image."""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAMES_SRC = ROOT / "assets" / "dos" / "games"
AMMO_GAMES = ROOT / "assets" / "dos" / "ammo" / "GAMES"
MANIFEST = GAMES_SRC / "games_manifest.json"
from rtx_dos_brand import HD_IMAGE  # noqa: E402

HD = ROOT / "assets" / "dos" / HD_IMAGE


def main() -> int:
    install = ROOT / "scripts" / "install_abandonware_dos.py"
    if install.is_file():
        subprocess.run([sys.executable, str(install)], cwd=ROOT, check=False)

    if AMMO_GAMES.is_dir():
        shutil.rmtree(AMMO_GAMES)
    AMMO_GAMES.mkdir(parents=True, exist_ok=True)

    if MANIFEST.is_file():
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        for game in data.get("games", []):
            gid = game.get("id", "")
            src = GAMES_SRC / gid
            if not src.is_dir():
                continue
            dst = AMMO_GAMES / gid.upper()
            shutil.copytree(src, dst)
            print(f"  ammo/GAMES/{gid.upper()}/ ({len(list(dst.iterdir()))} files)")

    mk = ROOT / "scripts" / "mk_ammo_hd.py"
    if mk.is_file():
        subprocess.run([sys.executable, str(mk), str(HD)], cwd=ROOT, check=True)
        pack = ROOT / "scripts" / "pack_dos_embed.py"
        if pack.is_file():
            subprocess.run([sys.executable, str(pack)], cwd=ROOT, check=False)
        for deploy in (
            ROOT / "build" / "bin" / "Linux" / "assets" / "dos",
            ROOT / "build" / "assets" / "dos",
        ):
            if deploy.parent.is_dir():
                deploy.mkdir(parents=True, exist_ok=True)
                shutil.copy2(HD, deploy / HD_IMAGE)

    print(f"OK staged games → {AMMO_GAMES} + rebuilt {HD}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())