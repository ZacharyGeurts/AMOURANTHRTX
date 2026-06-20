#!/usr/bin/env python3
"""Extract path-only metadata from /tmp/gh_tiny/t*.json for MCP push."""
import json
import sys
from pathlib import Path

OUT = Path("/tmp/gh_tiny/meta")
OUT.mkdir(parents=True, exist_ok=True)
start, end = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) >= 3 else (1, 27)
for i in range(start, end + 1):
    src = Path(f"/tmp/gh_tiny/t{i:03d}.json")
    if not src.is_file():
        continue
    batch = json.loads(src.read_text(encoding="utf-8"))
    meta = {
        "owner": "ZacharyGeurts",
        "repo": "AMOURANTHRTX",
        "branch": "main",
        "message": batch["message"],
        "paths": [f["path"] for f in batch["files"]],
        "skip_paths": ["AmmoOS/core/FieldAmouranthOs.hpp"],
    }
    (OUT / f"t{i:03d}.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    print(f"t{i:03d}: {len(meta['paths'])} files — {meta['message']}")