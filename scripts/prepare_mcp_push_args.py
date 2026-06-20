#!/usr/bin/env python3
"""Write per-batch MCP push_files args from /tmp/gh_tiny/t*.json."""
from __future__ import annotations

import json
import os
import sys

OUT = os.environ.get("GITHUB_SYNC_OUT", "/tmp/gh_tiny")
DEST = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".mcp_push")
SKIP = {"AmmoOS/core/FieldAmouranthOs.hpp"}
OWNER, REPO, BRANCH = "ZacharyGeurts", "AMOURANTHRTX", "main"


def main(argv: list[str]) -> int:
    start = int(argv[1]) if len(argv) > 1 else 1
    end = int(argv[2]) if len(argv) > 2 else 27
    os.makedirs(DEST, exist_ok=True)
    index: list[dict] = []
    for i in range(start, end + 1):
        src = os.path.join(OUT, f"t{i:03d}.json")
        if not os.path.isfile(src):
            index.append({"batch": f"t{i:03d}", "error": "missing"})
            continue
        with open(src, encoding="utf-8") as f:
            batch = json.load(f)
        files = [x for x in batch["files"] if x["path"] not in SKIP]
        args = {
            "owner": OWNER,
            "repo": REPO,
            "branch": BRANCH,
            "message": batch["message"],
            "files": files,
        }
        dest = os.path.join(DEST, f"t{i:03d}_args.json")
        with open(dest, "w", encoding="utf-8") as f:
            json.dump(args, f, ensure_ascii=False)
        index.append({
            "batch": f"t{i:03d}",
            "message": batch["message"],
            "files": len(files),
            "args_path": dest,
            "bytes": os.path.getsize(dest),
        })
        print(f"t{i:03d}: {len(files)} files -> {dest} ({os.path.getsize(dest)} bytes)")
    idx_path = os.path.join(DEST, "index.json")
    with open(idx_path, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))