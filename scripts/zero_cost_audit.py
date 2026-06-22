#!/usr/bin/env python3
"""Zero-cost path audit — verifies gated probes + core QA metrics stay green."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def run_qa(bin_name: str, timeout: int = 180) -> tuple[int, dict[str, str]]:
    exe = BUILD / bin_name
    if not exe.is_file():
        return 1, {}
    proc = subprocess.run(
        [str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=timeout, check=False,
    )
    metrics: dict[str, str] = {}
    for line in (proc.stdout + proc.stderr).splitlines():
        m = re.match(r"METRIC (\w+)=(.+)", line.strip())
        if m:
            metrics[m.group(1)] = m.group(2)
    return proc.returncode, metrics


def audit_gated_probes() -> bool:
    pipeline = ROOT / "Navigator/engine/Pipeline.hpp"
    if not pipeline.is_file():
        print("FAIL missing Pipeline.hpp", file=sys.stderr)
        return False
    text = pipeline.read_text(encoding="utf-8", errors="replace")
    gates = ("rtxProbesEnabled", "zero cost", "zero-cost")
    ok = all(g in text for g in gates)
    print(f"METRIC probe_gates={'1' if ok else '0'}")
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-qa", action="store_true", help="Only static gate checks")
    args = parser.parse_args()

    if not audit_gated_probes():
        return 1

    if args.skip_qa:
        print("OK zero_cost_audit static gates")
        return 0

    checks = (
        ("qa_fieldstorage_test", "vfs_bridge_ok", lambda v: v == "1", 60),
        ("qa_keen_host_test", "keen_fb_nz", lambda v: int(v) >= 500, 120),
        ("qa_doom_host_test", "doom_fb_nz", lambda v: int(v) >= 5000, 300),
    )
    for bin_name, metric, pred, timeout in checks:
        rc, metrics = run_qa(bin_name, timeout=timeout)
        if rc != 0:
            print(f"FAIL {bin_name} exit={rc}", file=sys.stderr)
            return 1
        if metric not in metrics or not pred(metrics[metric]):
            print(f"FAIL {bin_name} metric {metric}={metrics.get(metric)}", file=sys.stderr)
            return 1
        print(f"METRIC audit_{metric}={metrics[metric]}")

    print("OK zero_cost_audit gates + QA metrics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())