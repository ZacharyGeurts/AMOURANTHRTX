#!/usr/bin/env python3
"""AMOURANTHRTX 2.0 release checklist — full polish QA matrix."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"

os.environ["AMOURANTHRTX_END_GAME"] = "1"
os.environ["AMOURANTHRTX_EVERYTHING_EVERYWHERE"] = "1"
os.environ["AMOURANTHRTX_FIELD_PERSIST"] = "1"
os.environ["AMOURANTHRTX_INFINITE"] = "1"
os.environ["AMOURANTHRTX_ALL_BREAKTHROUGHS"] = "1"


def run_py(script: str, *args: str) -> int:
    return subprocess.run(
        [sys.executable, str(ROOT / "scripts" / script), *args],
        cwd=ROOT, check=False,
    ).returncode


def ensure_build() -> int:
    if (BUILD / "qa_keen_host_test").is_file():
        return 0
    return subprocess.run(
        ["cmake", "--build", str(BUILD), "-j", "8"], cwd=ROOT, check=False,
    ).returncode


def main() -> int:
    from ammo_platform import AMOURANTHRTX_VERSION, MANIFEST_VERSION  # noqa: E402

    print(f"METRIC amouranthrtx_version={AMOURANTHRTX_VERSION}")
    print(f"METRIC manifest_version={MANIFEST_VERSION}")

    if ensure_build() != 0:
        print("FAIL release build", file=sys.stderr)
        return 1

    py_steps = (
        ("monolith_audit.py", ()),
        ("zero_cost_audit.py", ("--end-game",)),
        ("bench_mame_compare.py", ()),
        ("bench_chips.py", ()),
        ("play_legacy.py", ()),
        ("team_drive_test.py", ()),
        ("bench_storage.py", ()),
    )
    py_extra = (
        "qa_ocr_click_test.py",
        "qa_aos_ocr_test.py",
    )

    bin_steps = (
        "qa_keen_host_test",
        "qa_field_persist_test",
        "qa_rtx_density_test",
        "qa_love_demo_test",
        "qa_aos_gui_test",
        "qa_font_sdf_test",
        "qa_taskbar_click_test",
        "qa_amouranthos_test",
        "qa_hostess_native_test",
    )

    for script, args in py_steps:
        if run_py(script, *args) != 0:
            print(f"FAIL release_checklist {script}", file=sys.stderr)
            return 1
        print(f"OK release_checklist {script}")

    for script in py_extra:
        if run_py(script) != 0:
            print(f"FAIL release_checklist {script}", file=sys.stderr)
            return 1
        print(f"OK release_checklist {script}")

    if run_py("field_superintelligence.py", "evaluate") != 0:
        print("FAIL release_checklist field_superintelligence evaluate", file=sys.stderr)
        return 1
    print("OK release_checklist field_superintelligence evaluate")

    if run_py("field_superintelligence.py", "turnover") != 0:
        print("FAIL release_checklist field_superintelligence turnover", file=sys.stderr)
        return 1
    print("OK release_checklist field_superintelligence turnover")

    for step in bin_steps:
        subprocess.run(
            ["cmake", "--build", str(BUILD), "--target", step, "-j", "8"],
            cwd=ROOT, check=True,
        )
        env = {**os.environ}
        if step == "qa_love_demo_test":
            env.setdefault("AMOURANTHRTX_EVERYTHING_EVERYWHERE", "1")
            env.setdefault("AMOURANTHRTX_FIELD_PERSIST", "1")
        if step == "qa_hostess_native_test":
            env.setdefault("AMOURANTHRTX_HOSTESS", "1")
        rc = subprocess.run([str(BUILD / step)], cwd=ROOT, check=False, env=env).returncode
        if rc != 0:
            print(f"FAIL release_checklist {step}", file=sys.stderr)
            return 1
        print(f"OK release_checklist {step}")

    print("METRIC release_2_0=1")
    print("OK release_checklist_2_0 GREEN ALL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())