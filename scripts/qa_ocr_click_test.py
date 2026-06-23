#!/usr/bin/env python3
"""QA: OCR-verified taskbar click targets — layout vs ink centroid at 4K."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from ocr_click_toolkit import (  # noqa: E402
    calibrate_from_png,
    capture_snap,
    ensure_pillow,
    validate_report,
    write_calibration,
)


def main() -> int:
    ensure_pillow()
    engine = ROOT / "build" / "bin" / "Linux" / "AMOURANTHRTX"
    if not engine.is_file():
        subprocess.run(["./linux.sh"], cwd=ROOT, check=True, timeout=600)
    else:
        subprocess.run(
            ["cmake", "--build", str(ROOT / "build"), "--target", "amouranth_engine", "copy_assets", "-j8"],
            cwd=ROOT, check=True, timeout=300,
        )

    print("=== QA: OCR click calibration (4K shell snap) ===")
    png = capture_snap(engine, "qa_ocr_click", w=3840, h=2160)
    report = calibrate_from_png(png, vp_w=3840, vp_h=2160)
    write_calibration(report)

    errors = validate_report(report)
    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1

    print("=== QA: C++ taskbar hit test (layout clicks) ===")
    taskbar = ROOT / "build" / "qa_taskbar_click_test"
    if not taskbar.is_file():
        subprocess.run(
            ["cmake", "--build", str(ROOT / "build"), "--target", "qa_taskbar_click_test", "-j8"],
            cwd=ROOT, check=True,
        )
    rc = subprocess.run([str(taskbar)], cwd=ROOT, check=False).returncode
    if rc != 0:
        print("FAIL qa_taskbar_click_test", file=sys.stderr)
        return 1

    print("OK qa_ocr_click_test GREEN ALL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())