#!/usr/bin/env python3
"""Detect installed web browsers + OS default; write assets/browsers.json for Field hook."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "browsers.json"

CANDIDATES = (
    ("firefox", "Firefox", ["firefox", "firefox-esr"]),
    ("chromium", "Chromium", ["chromium", "chromium-browser", "google-chrome", "chrome"]),
    ("brave", "Brave", ["brave-browser", "brave"]),
    ("edge", "Microsoft Edge", ["microsoft-edge", "microsoft-edge-stable"]),
    ("opera", "Opera", ["opera"]),
    ("vivaldi", "Vivaldi", ["vivaldi", "vivaldi-stable"]),
    ("waterfox", "Waterfox", ["waterfox", "waterfox-bin"]),
    ("librewolf", "LibreWolf", ["librewolf"]),
    ("tor", "Tor Browser", ["tor-browser", "torbrowser-launcher"]),
)

DESKTOP_TO_ID = {
    "firefox.desktop": "firefox",
    "firefox-esr.desktop": "firefox",
    "chromium-browser.desktop": "chromium",
    "chromium.desktop": "chromium",
    "google-chrome.desktop": "chromium",
    "google-chrome-stable.desktop": "chromium",
    "brave-browser.desktop": "brave",
    "microsoft-edge.desktop": "edge",
    "microsoft-edge-stable.desktop": "edge",
    "opera.desktop": "opera",
    "vivaldi-stable.desktop": "vivaldi",
    "org.mozilla.firefox.desktop": "firefox",
}


def resolve(bin_name: str) -> str | None:
    path = shutil.which(bin_name)
    if path:
        return path
    for base in ("/usr/bin", "/usr/local/bin", "/snap/bin", os.path.expanduser("~/.local/bin")):
        p = Path(base) / bin_name
        if p.is_file() and os.access(p, os.X_OK):
            return str(p)
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


def os_default_desktop() -> str | None:
    for cmd in (
        ["xdg-settings", "get", "default-web-browser"],
        ["xdg-mime", "query", "default", "x-scheme-handler/http"],
        ["xdg-mime", "query", "default", "text/html"],
    ):
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=5, check=False)
            line = (proc.stdout or "").strip()
            if proc.returncode == 0 and line.endswith(".desktop"):
                return line
        except (OSError, subprocess.TimeoutExpired):
            continue
    return None


def desktop_exec(desktop_file: str) -> str | None:
    paths = [
        Path(f"/usr/share/applications/{desktop_file}"),
        Path(f"/usr/local/share/applications/{desktop_file}"),
        Path.home() / ".local/share/applications" / desktop_file,
    ]
    snap = re.match(r"^(chromium|firefox|brave-browser|microsoft-edge)_([^/]+)\.desktop$", desktop_file)
    if snap:
        paths.insert(0, Path(f"/var/lib/snapd/desktop/applications/{desktop_file}"))
    for p in paths:
        if not p.is_file():
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for line in text.splitlines():
            if line.startswith("Exec="):
                raw = line[5:].strip()
                raw = re.sub(r"\s%[uUfF]", "", raw)
                raw = raw.split()[0] if raw else ""
                if raw and not raw.startswith("/"):
                    found = resolve(raw)
                    if found:
                        return found
                if raw.startswith("/") and os.access(raw, os.X_OK):
                    return raw
    return None


def detect() -> list[dict[str, object]]:
    found: list[dict[str, object]] = []
    seen: set[str] = set()
    for key, label, bins in CANDIDATES:
        for b in bins:
            path = resolve(b)
            if not path or path in seen:
                continue
            seen.add(path)
            found.append({
                "id": key,
                "label": label,
                "binary": path,
                "argv0": b,
                "version": version_of(path),
                "default": False,
            })
            break

    default_desktop = os_default_desktop()
    default_id: str | None = None
    default_binary: str | None = None
    if default_desktop:
        default_id = DESKTOP_TO_ID.get(default_desktop)
        default_binary = desktop_exec(default_desktop)
        if not default_id:
            base = default_desktop.lower()
            for key, _, _ in CANDIDATES:
                if key in base:
                    default_id = key
                    break

    if default_binary:
        for entry in found:
            if entry["binary"] == default_binary:
                entry["default"] = True
                default_id = str(entry["id"])
                break
        else:
            found.insert(0, {
                "id": default_id or "default",
                "label": default_desktop.replace(".desktop", "").replace("-", " ").title(),
                "binary": default_binary,
                "argv0": Path(default_binary).name,
                "version": version_of(default_binary),
                "default": True,
            })

    if default_id:
        for entry in found:
            entry["default"] = entry["id"] == default_id

    found.sort(key=lambda e: (not bool(e.get("default")), str(e.get("label", ""))))
    return found


def main() -> int:
    browsers = detect()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    default_entry = next((b for b in browsers if b.get("default")), None)
    payload = {
        "generated": True,
        "default_id": default_entry["id"] if default_entry else None,
        "default_binary": default_entry["binary"] if default_entry else None,
        "browsers": browsers,
    }
    OUT.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    if not browsers:
        print("No browsers detected — install firefox or chromium", file=sys.stderr)
        return 1
    if default_entry:
        print(f"  OS default: {default_entry['label']} ({default_entry['binary']})")
    for b in browsers:
        mark = " *" if b.get("default") else ""
        print(f"  {b['label']}: {b['binary']}{mark}")
    print(f"Wrote {OUT} ({len(browsers)} browser(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())