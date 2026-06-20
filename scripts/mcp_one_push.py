#!/usr/bin/env python3
"""Emit push_files JSON for one local file (sequential MCP sync)."""
from __future__ import annotations

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWNER, REPO, BRANCH = "ZacharyGeurts", "AMOURANTHRTX", "main"
OUT = os.environ.get("GITHUB_SYNC_ONE", "/tmp/gh_tiny/one_push.json")


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: mcp_one_push.py <repo-relative-path> [commit-message]", file=sys.stderr)
        return 2
    rel = sys.argv[1].replace("\\", "/").lstrip("/")
    msg = sys.argv[2] if len(sys.argv) > 2 else f"Sync: {rel}"
    local = os.path.join(ROOT, rel)
    if not os.path.isfile(local):
        print(f"missing: {local}", file=sys.stderr)
        return 1
    with open(local, encoding="utf-8") as f:
        content = f.read()
    args = {
        "owner": OWNER,
        "repo": REPO,
        "branch": BRANCH,
        "message": msg,
        "files": [{"path": rel, "content": content}],
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(args, f, ensure_ascii=False)
    print(OUT)
    print(f"bytes={len(content.encode('utf-8'))} lines={content.count(chr(10))+1}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())