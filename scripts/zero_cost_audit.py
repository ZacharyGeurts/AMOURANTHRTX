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


def run_qa(bin_name: str, timeout: int = 180, env: dict[str, str] | None = None) -> tuple[int, dict[str, str]]:
    exe = BUILD / bin_name
    if not exe.is_file():
        return 1, {}
    proc = subprocess.run(
        [str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=timeout, check=False,
        env={**dict(__import__("os").environ), **(env or {})},
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
    parser.add_argument("--end-game", action="store_true", help="End-game matrix (fast, skips doom)")
    parser.add_argument("--with-doom", action="store_true", help="Include doom QA even in end-game mode")
    args = parser.parse_args()

    if not audit_gated_probes():
        return 1

    if args.skip_qa:
        print("OK zero_cost_audit static gates")
        return 0

    end_env = {"AMOURANTHRTX_END_GAME": "1"} if args.end_game else None
    everything_env = {
        **(end_env or {}),
        "AMOURANTHRTX_EVERYTHING_EVERYWHERE": "1",
        "AMOURANTHRTX_FIELD_PERSIST": "1",
    }

    if args.end_game:
        suites: tuple[tuple[str, int, tuple[tuple[str, object], ...]], ...] = (
            ("qa_fieldstorage_test", 60, (
                ("vfs_bridge_ok", lambda v: v == "1"),
                ("dual_host", lambda v: v == "1"),
                ("hyper_breakthroughs", lambda v: v == "1"),
                ("hyper_fabric_scale", lambda v: float(v) > 1.0),
                ("hyper_phi_super", lambda v: float(v) > 0.0),
            )),
            ("qa_ps1_test", 30, (("ps1_gpu_wave", lambda v: v == "1"),)),
            ("qa_xbox360_test", 30, (("xbox360_gpu_wave", lambda v: v == "1"),)),
            ("qa_amiga_test", 30, (("amiga_love_score", lambda v: int(v) >= 10),)),
            ("qa_n64_test", 30, (("n64_gpu_wave", lambda v: v == "1"),)),
            ("qa_dreamcast_test", 30, (("dreamcast_gpu_wave", lambda v: v == "1"),)),
            ("qa_ps2_test", 30, (("ps2_gpu_wave", lambda v: v == "1"),)),
            ("qa_everything_test", 60, (
                ("everything_active", lambda v: v == "1"),
                ("field_persist_restored", lambda v: v == "1"),
            )),
            ("qa_field_persist_test", 60, (
                ("field_data_intact", lambda v: v == "1"),
                ("field_persist_restored", lambda v: v == "1"),
            )),
            ("qa_rtx_density_test", 30, (("rtx_density_ok", lambda v: v == "1"),)),
            ("qa_love_demo_test", 60, (("love_demo_complete", lambda v: v == "1"),)),
            ("qa_keen_host_test", 120, (
                ("keen_ip_progress", lambda v: v == "1"),
                ("keen_title_native", lambda v: v == "1"),
                ("keen_title_match", lambda v: v == "1"),
                ("keen_fb_nz", lambda v: int(v) >= 500),
            )),
        )
        if args.with_doom:
            suites = (*suites, ("qa_doom_host_test", 300, (("doom_fb_nz", lambda v: int(v) >= 5000),)))
    else:
        suites = (
            ("qa_fieldstorage_test", 60, (("hyper_breakthroughs", lambda v: v == "1"),)),
            ("qa_ps1_test", 30, (("ps1_gpu_wave", lambda v: v == "1"),)),
            ("qa_xbox360_test", 30, (("xbox360_gpu_wave", lambda v: v == "1"),)),
            ("qa_amiga_test", 30, (("amiga_love_score", lambda v: int(v) >= 10),)),
            ("qa_keen_host_test", 120, (("keen_fb_nz", lambda v: int(v) >= 500),)),
            ("qa_doom_host_test", 300, (("doom_fb_nz", lambda v: int(v) >= 5000),)),
        )

    for bin_name, timeout, metric_checks in suites:
        env = everything_env if bin_name == "qa_everything_test" else end_env
        rc, metrics = run_qa(bin_name, timeout=timeout, env=env)
        if rc != 0:
            print(f"FAIL {bin_name} exit={rc}", file=sys.stderr)
            return 1
        for metric, pred in metric_checks:
            if metric not in metrics or not pred(metrics[metric]):
                print(f"FAIL {bin_name} metric {metric}={metrics.get(metric)}", file=sys.stderr)
                return 1
            print(f"METRIC audit_{metric}={metrics[metric]}")

    if args.end_game:
        print("METRIC end_game_mode=1")
        print("OK zero_cost_audit end-game gates + QA metrics GREEN ALL")
    else:
        print("OK zero_cost_audit gates + QA metrics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())