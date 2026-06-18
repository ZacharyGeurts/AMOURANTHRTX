#!/usr/bin/env python3
"""Emit GitHub sync batches (push + delete lists) for AMOURANTHRTX."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWNER, REPO, BRANCH = "ZacharyGeurts", "AMOURANTHRTX", "main"
OUT = os.environ.get("GITHUB_SYNC_OUT", "/tmp/gh_tiny")


def git_files() -> set[str]:
    files = set(
        subprocess.check_output(["git", "ls-files"], cwd=ROOT, text=True).strip().split("\n")
    )
    for p in ("Toolchain-emscripten.cmake", "Toolchain-mingw64.cmake"):
        if os.path.isfile(os.path.join(ROOT, p)):
            files.add(p)
    return files


def remote_blobs() -> dict[str, dict]:
    url = f"https://api.github.com/repos/{OWNER}/{REPO}/git/trees/{BRANCH}?recursive=1"
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json", "User-Agent": "github-sync"})
    with urllib.request.urlopen(req) as r:
        data = json.load(r)
    return {i["path"]: i for i in data["tree"] if i["type"] == "blob"}


def read_content(path: str) -> str:
    with open(os.path.join(ROOT, path), "rb") as f:
        raw = f.read()
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def main() -> int:
    local = git_files()
    remote = remote_blobs()
    to_push = []
    for p in sorted(local):
        fp = os.path.join(ROOT, p)
        if not os.path.isfile(fp):
            continue
        sz = os.path.getsize(fp)
        if p not in remote or remote[p].get("size") != sz:
            to_push.append(p)

    os.makedirs(OUT, exist_ok=True)
    batches: list[list[dict]] = []
    cur: list[dict] = []
    cur_size = 0
    for p in sorted(to_push, key=lambda x: os.path.getsize(os.path.join(ROOT, x))):
        content = read_content(p)
        est = len(content.encode("utf-8", errors="surrogateescape"))
        if cur and (cur_size + est > 40000 or len(cur) >= 6):
            batches.append(cur)
            cur, cur_size = [], 0
        cur.append({"path": p, "content": content})
        cur_size += est
    if cur:
        batches.append(cur)

    for i, batch in enumerate(batches):
        payload = {"message": f"Sync AMOURANTHRTX ({i + 1}/{len(batches)})", "files": batch}
        with open(os.path.join(OUT, f"t{i:03d}.json"), "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False)

    delete = sorted(set(remote) - local)
    with open(os.path.join(OUT, "delete.json"), "w", encoding="utf-8") as f:
        json.dump(delete, f, indent=2)

    print(f"root={ROOT}")
    print(f"push={len(to_push)} batches={len(batches)} delete={len(delete)} out={OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())