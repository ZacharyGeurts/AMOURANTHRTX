#!/usr/bin/env python3
"""Push t001..t027 via GitHub git API; write per-batch results to mcp_push_results.json."""
from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from github_push_batches import push_batch  # noqa: E402

OUT = os.environ.get("GITHUB_SYNC_OUT", "/tmp/gh_tiny")
SKIP = {"AmmoOS/core/FieldAmouranthOs.hpp"}
RESULTS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mcp_push_results.json")


def main(argv: list[str]) -> int:
    start = int(argv[1]) if len(argv) > 1 else 1
    end = int(argv[2]) if len(argv) > 2 else 27
    pushed: list[dict] = []
    failures: list[dict] = []
    for i in range(start, end + 1):
        src = os.path.join(OUT, f"t{i:03d}.json")
        if not os.path.isfile(src):
            failures.append({"batch": f"t{i:03d}", "error": "missing"})
            continue
        with open(src, encoding="utf-8") as f:
            batch = json.load(f)
        batch["files"] = [x for x in batch["files"] if x["path"] not in SKIP]
        if not batch["files"]:
            pushed.append({"batch": f"t{i:03d}", "sha": None, "skipped": "empty"})
            continue
        tmp = src + ".push.json"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(batch, f, ensure_ascii=False)
        try:
            sha = push_batch(tmp)
            pushed.append({"batch": f"t{i:03d}", "sha": sha, "message": batch["message"]})
            print(f"ok t{i:03d} -> {sha}")
        except Exception as e:
            failures.append({"batch": f"t{i:03d}", "error": str(e)})
            print(f"FAIL t{i:03d}: {e}", file=sys.stderr)
        finally:
            if os.path.isfile(tmp):
                os.remove(tmp)
    out = {"pushed": pushed, "failures": failures, "final_head": pushed[-1]["sha"] if pushed and pushed[-1].get("sha") else None}
    with open(RESULTS, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))