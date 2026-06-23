#!/usr/bin/env python3
"""Field infinite brain — persistent local memory on TEAM drive / fieldstorage.

Stores agent + session notes in cache/fieldstorage/brain/ (resonance hold alongside
field_wave.persist). Run with AMOURANTHRTX_INFINITE + AMOURANTHRTX_FIELD_PERSIST for
live FieldStorage coupling.
"""
from __future__ import annotations

import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STORAGE = ROOT / "cache" / "fieldstorage"
BRAIN = STORAGE / "brain"
MEMORY_FILE = BRAIN / "grok_memory.jsonl"
INDEX_FILE = BRAIN / "index.json"
TEAM_DEV = os.environ.get("TEAM_DRIVE_DEV", "/dev/nvme2n1")


def setup() -> int:
    BRAIN.mkdir(parents=True, exist_ok=True)
    staging = STORAGE / "team_staging"
    staging.mkdir(parents=True, exist_ok=True)
    marker = staging / "TEAM_DRIVE_OK"
    marker.write_text(f"device={TEAM_DEV}\nbrain={BRAIN}\n", encoding="utf-8")
    if not MEMORY_FILE.is_file():
        MEMORY_FILE.write_text("", encoding="utf-8")
    idx = {
        "version": 1,
        "team_device": TEAM_DEV,
        "brain_root": str(BRAIN),
        "field_persist": str(STORAGE / "field_wave.persist"),
        "team_image": str(STORAGE / "team_drive.img"),
        "created": datetime.now(timezone.utc).isoformat(),
    }
    INDEX_FILE.write_text(json.dumps(idx, indent=2) + "\n", encoding="utf-8")
    print(f"METRIC brain_root={BRAIN}")
    print(f"METRIC team_device={TEAM_DEV}")
    print(f"METRIC memory_file={MEMORY_FILE}")
    print(f"METRIC field_persist={STORAGE / 'field_wave.persist'}")
    print("OK field_brain_memory setup")
    return 0


def remember(text: str, *, tag: str = "note") -> int:
    setup()
    entry = {
        "ts": datetime.now(timezone.utc).isoformat(),
        "tag": tag,
        "text": text.strip(),
    }
    with MEMORY_FILE.open("a", encoding="utf-8") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")
    print(f"OK remember tag={tag} bytes={len(text)}")
    return 0


def recall(limit: int = 20) -> int:
    if not MEMORY_FILE.is_file():
        print("OK recall empty")
        return 0
    lines = MEMORY_FILE.read_text(encoding="utf-8").strip().splitlines()
    for line in lines[-limit:]:
        print(line)
    print(f"METRIC recall_count={min(limit, len(lines))}")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        return setup()
    cmd = sys.argv[1]
    if cmd == "setup":
        return setup()
    if cmd == "remember" and len(sys.argv) >= 3:
        return remember(" ".join(sys.argv[2:]), tag=os.environ.get("BRAIN_TAG", "note"))
    if cmd == "recall":
        limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20
        return recall(limit)
    print("usage: field_brain_memory.py [setup|remember <text>|recall [n]]", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())