#!/usr/bin/env python3
"""Emit push_files JSON for the three Navigator engine files."""
import json
import sys

ROOT = "/home/default/Desktop/SG/AMOURANTHRTX"
FILES = [
    "Navigator/engine/FieldCdRomHost.cpp",
    "Navigator/engine/FieldCdRomHost.hpp",
    "Navigator/engine/FieldAmmoNes.hpp",
]
args = {
    "owner": "ZacharyGeurts",
    "repo": "AMOURANTHRTX",
    "branch": "main",
    "message": "Sync: AmmoNES CHIPS refactor shell + FieldCdRomHost",
    "files": [],
}
for rel in FILES:
    with open(f"{ROOT}/{rel}", encoding="utf-8") as f:
        args["files"].append({"path": rel, "content": f.read()})
out = "/tmp/mcp_push_three.json"
with open(out, "w", encoding="utf-8") as f:
    json.dump(args, f, ensure_ascii=False)
print(out)