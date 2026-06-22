#!/usr/bin/env python3
"""FieldStorage v2 R/W bench wrapper."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build" / "qa_fieldstorage_test"


def main() -> int:
    if not BIN.is_file():
        subprocess.check_call(
            ["cmake", "--build", str(ROOT / "build"), "--target", "qa_fieldstorage_test", "-j4"],
            cwd=ROOT,
        )
    proc = subprocess.run([str(BIN)], cwd=ROOT, capture_output=True, text=True)
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())