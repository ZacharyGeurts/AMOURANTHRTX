#!/usr/bin/env python3
"""TEAM drive harness — uses empty nvme2n1 only; never touches boot/data drives."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTECTED = {"nvme0n1", "sda", "sdb", "nvme1n1"}
DEFAULT_TEAM = "/dev/nvme2n1"


def lsblk_map() -> dict[str, str]:
    out = subprocess.check_output(["lsblk", "-ndo", "NAME,TYPE"], text=True)
    return {line.split()[0]: line.split()[1] for line in out.splitlines() if line.strip()}


def main() -> int:
    dev = os.environ.get("TEAM_DRIVE_DEV", DEFAULT_TEAM)
    base = Path(dev).name
    if base in PROTECTED:
        print(f"FAIL protected device {dev}", file=sys.stderr)
        return 1

    table = lsblk_map()
    if base not in table:
        if os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS"):
            staging = ROOT / "cache" / "fieldstorage" / "team_staging"
            staging.mkdir(parents=True, exist_ok=True)
            print("METRIC team_skip_ci=1")
            print(f"METRIC team_staging={staging}")
            print("OK team drive harness skipped (no nvme2n1 in CI runner)")
            return 0
        print(f"FAIL device missing {dev}", file=sys.stderr)
        return 1
    if table[base] != "disk":
        print(f"FAIL not a disk {dev}", file=sys.stderr)
        return 1

    staging = ROOT / "cache" / "fieldstorage" / "team_staging"
    staging.mkdir(parents=True, exist_ok=True)
    marker = staging / "TEAM_DRIVE_OK"
    marker.write_text(f"device={dev}\n", encoding="utf-8")

    print(f"METRIC team_device={dev}")
    print(f"METRIC team_staging={staging}")
    print(f"METRIC team_parts={sum(1 for k in table if k.startswith(base))}")
    print("OK team drive harness (non-destructive staging)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())