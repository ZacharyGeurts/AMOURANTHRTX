#!/usr/bin/env bash
# Emit batch metadata for MCP push_files (run batches via agent or MCP client).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${GITHUB_SYNC_OUT:-/tmp/gh_tiny}"
python3 "$ROOT/scripts/github_sync_batches.py"
for i in $(seq 0 28); do
  f=$(printf '%s/t%03d.json' "$OUT" "$i")
  [[ -f "$f" ]] || continue
  python3 - "$f" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
args = {
    "owner": "ZacharyGeurts",
    "repo": "AMOURANTHRTX",
    "branch": "main",
    "message": d["message"],
    "files": d["files"],
}
print(json.dumps({"batch": sys.argv[1], "count": len(d["files"]), "message": d["message"]}))
PY
done
echo "delete_list=$OUT/delete.json"