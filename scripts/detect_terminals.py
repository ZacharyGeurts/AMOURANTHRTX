#!/usr/bin/env python3
"""Detect installed host terminals (Mint/Cinnamon default first); write assets/terminals.json."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "terminals.json"

CANDIDATES = (
    ("gnome-terminal", "GNOME Terminal", ["gnome-terminal", "x-terminal-emulator"]),
    ("mate-terminal", "MATE Terminal", ["mate-terminal"]),
    ("xfce4-terminal", "Xfce Terminal", ["xfce4-terminal"]),
    ("konsole", "Konsole", ["konsole"]),
    ("lxterminal", "LXTerminal", ["lxterminal"]),
    ("xterm", "XTerm", ["xterm"]),
)


def resolve(bin_name: str) -> str | None:
    path = shutil.which(bin_name)
    if path:
        path = os.path.realpath(path)
        if path.endswith(".wrapper"):
            alt = shutil.which("gnome-terminal")
            if alt:
                path = os.path.realpath(alt)
        return path
    for base in ("/usr/bin", "/usr/local/bin", "/snap/bin", os.path.expanduser("~/.local/bin")):
        p = Path(base) / bin_name
        if p.is_file() and os.access(p, os.X_OK):
            return str(p.resolve())
    return None


def version_of(path: str) -> str:
    for flag in ("--version", "-version"):
        try:
            proc = subprocess.run(
                [path, flag], capture_output=True, text=True, timeout=8, check=False,
            )
            line = (proc.stdout or proc.stderr or "").strip().split("\n", 1)[0]
            if line:
                return line[:120]
        except (OSError, subprocess.TimeoutExpired):
            continue
    return ""


def main() -> int:
    entries = []
    seen: set[str] = set()
    for tid, label, bins in CANDIDATES:
        for name in bins:
            path = resolve(name)
            if not path or path in seen:
                continue
            seen.add(path)
            entries.append({
                "id": tid,
                "label": label,
                "binary": path,
                "version": version_of(path),
                "default": False,
            })
            break

    default = entries[0] if entries else None
    if default:
        default["default"] = True

    payload = {
        "default_id": default["id"] if default else "",
        "default_binary": default["binary"] if default else "",
        "default_label": default["label"] if default else "",
        "terminals": entries,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    if default:
        print(f"Wrote {OUT} ({len(entries)} terminal(s), default={default['label']})")
    else:
        print(f"Wrote {OUT} (no host terminal found)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())