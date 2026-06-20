#!/usr/bin/env python3
"""Push /tmp/gh_tiny/t001..t027.json via GitHub git API (same as github_push_batches.py).

Usage:
  python3 scripts/mcp_push_all_batches.py [START] [END]

Requires GITHUB_TOKEN, GH_TOKEN, or GITHUB_PAT.
Skips FieldAmouranthOs.hpp if present (pushed separately).
"""
from __future__ import annotations

import json
import os
import sys

# Reuse low-level git-tree push from sibling script
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from github_push_batches import push_batch  # type: ignore

OUT = os.environ.get("GITHUB_SYNC_OUT", "/tmp/gh_tiny")
SKIP = {"AmmoOS/core/FieldAmouranthOs.hpp"}


def load_batch(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        batch = json.load(f)
    batch["files"] = [x for x in batch["files"] if x["path"] not in SKIP]
    return batch


def main(argv: list[str]) -> int:
    start = int(argv[1]) if len(argv) > 1 else 1
    end = int(argv[2]) if len(argv) > 2 else 27
    results: list[tuple[str, str]] = []
    failures: list[tuple[str, str]] = []
    for i in range(start, end + 1):
        path = os.path.join(OUT, f"t{i:03d}.json")
        if not os.path.isfile(path):
            print(f"skip missing {path}")
            continue
        try:
            batch = load_batch(path)
            if not batch["files"]:
                print(f"skip empty {path}")
                continue
            # push_batch expects file path; wrap temp json
            tmp = path + ".push.json"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(batch, f, ensure_ascii=False)
            sha = push_batch(tmp)
            os.remove(tmp)
            results.append((path, sha))
            print(f"ok {path} -> {sha}")
        except Exception as e:
            failures.append((path, str(e)))
            print(f"FAIL {path}: {e}", file=sys.stderr)
    print("\n=== summary ===")
    for path, sha in results:
        print(f"  {path}: {sha}")
    if failures:
        print("failures:")
        for path, err in failures:
            print(f"  {path}: {err}")
        return 1
    if results:
        print(f"final_head={results[-1][1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))