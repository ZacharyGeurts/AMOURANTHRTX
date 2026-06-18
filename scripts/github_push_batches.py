#!/usr/bin/env python3
"""Push prepared /tmp/gh_tiny/t*.json batches via GitHub Contents API.

Requires GITHUB_TOKEN or GH_TOKEN in the environment (repo scope).
"""
from __future__ import annotations

import base64
import json
import os
import sys
import urllib.error
import urllib.request

OWNER, REPO, BRANCH = "ZacharyGeurts", "AMOURANTHRTX", "main"
OUT = os.environ.get("GITHUB_SYNC_OUT", "/tmp/gh_tiny")


def token() -> str:
    for key in ("GITHUB_TOKEN", "GH_TOKEN", "GITHUB_PAT"):
        val = os.environ.get(key)
        if val:
            return val
    raise SystemExit("Set GITHUB_TOKEN, GH_TOKEN, or GITHUB_PAT")


def api(method: str, path: str, body: dict | None = None) -> dict:
    url = f"https://api.github.com{path}"
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(
        url,
        data=data,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token()}",
            "User-Agent": "amouranthrtx-sync",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(req) as resp:
        raw = resp.read()
    return json.loads(raw) if raw else {}


def file_sha(path: str) -> str | None:
    try:
        info = api("GET", f"/repos/{OWNER}/{REPO}/contents/{path}?ref={BRANCH}")
        return info.get("sha")
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def push_batch(batch_path: str) -> str:
    with open(batch_path, encoding="utf-8") as f:
        batch = json.load(f)
    tree = []
    for item in batch["files"]:
        path = item["path"]
        content = item["content"]
        if isinstance(content, str):
            blob = content.encode("utf-8", errors="surrogateescape")
        else:
            blob = content
        entry = {
            "path": path,
            "mode": "100644",
            "type": "blob",
            "content": base64.b64encode(blob).decode("ascii"),
        }
        sha = file_sha(path)
        if sha:
            entry["sha"] = sha
        tree.append(entry)

    ref = api("GET", f"/repos/{OWNER}/{REPO}/git/ref/heads/{BRANCH}")
    base_sha = ref["object"]["sha"]
    base_commit = api("GET", f"/repos/{OWNER}/{REPO}/git/commits/{base_sha}")
    new_tree = api(
        "POST",
        f"/repos/{OWNER}/{REPO}/git/trees",
        {"base_tree": base_commit["tree"]["sha"], "tree": tree},
    )
    new_commit = api(
        "POST",
        f"/repos/{OWNER}/{REPO}/git/commits",
        {
            "message": batch.get("message", "Sync AMOURANTHRTX"),
            "tree": new_tree["sha"],
            "parents": [base_sha],
        },
    )
    api(
        "PATCH",
        f"/repos/{OWNER}/{REPO}/git/refs/heads/{BRANCH}",
        {"sha": new_commit["sha"], "force": False},
    )
    return new_commit["sha"]


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(f"usage: {argv[0]} START END", file=sys.stderr)
        return 2
    start, end = int(argv[1]), int(argv[2])
    for i in range(start, end + 1):
        path = os.path.join(OUT, f"t{i:03d}.json")
        if not os.path.isfile(path):
            print(f"skip missing {path}")
            continue
        sha = push_batch(path)
        print(f"ok {path} -> {sha[:8]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))