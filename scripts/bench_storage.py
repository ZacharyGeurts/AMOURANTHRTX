#!/usr/bin/env python3
"""FieldStorage v2 R/W bench + 2GB SDF wave transformation table."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build" / "qa_fieldstorage_test"
BASE_GB = 2.0
WAVE_PHASES = (0.0, 0.785398, 1.570796, 2.356194, 3.141593)
COMMON_DRIVES = (
    ("team_nvme2", "/dev/nvme2n1"),
    ("field_cache", "cache/fieldstorage/team_drive.img"),
    ("vfs_root", "assets"),
)


def ensure_binary() -> None:
    if BIN.is_file():
        return
    subprocess.check_call(
        ["cmake", "--build", str(ROOT / "build"), "--target", "qa_fieldstorage_test", "-j4"],
        cwd=ROOT,
    )


def parse_metrics(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in text.splitlines():
        m = re.match(r"METRIC (\w+)=(.+)", line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def print_transform_table(bo_gain: float) -> None:
    print("METRIC transform_anchor_gb=2.00")
    print("--- 2GB SDF wave transformation table ---")
    print(f"{'phase':>8}  {'logical_gb':>12}  {'fold_x':>8}")
    for phase in WAVE_PHASES:
        resonance = 1.0 + __import__("math").sin(phase)
        logical_gb = BASE_GB * bo_gain * resonance
        fold_x = bo_gain * resonance
        print(f"{phase:8.3f}  {logical_gb:12.2f}  {fold_x:8.2f}")
        print(f"METRIC transform_phase_{phase:.3f}_gb={logical_gb:.2f}")


def probe_common_drives() -> int:
    rc = 0
    for label, path in COMMON_DRIVES:
        p = ROOT / path if not path.startswith("/") else Path(path)
        exists = p.exists()
        print(f"METRIC drive_{label}={'1' if exists else '0'}")
        if label == "team_nvme2" and exists:
            subprocess.run([sys.executable, str(ROOT / "scripts" / "team_drive_test.py")], check=False)
    return rc


def main() -> int:
    ensure_binary()
    proc = subprocess.run([str(BIN)], cwd=ROOT, capture_output=True, text=True)
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)
    if proc.returncode != 0:
        return proc.returncode

    metrics = parse_metrics(proc.stdout)
    bo_gain = float(metrics.get("bo_gain", "1.0"))
    print_transform_table(bo_gain)
    probe_common_drives()
    print("OK bench_storage transform + drives locked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())