#!/usr/bin/env python3
"""AMOURANTHRTX SuperIntelligence — offline Field brain on infinite storage.

All thinking lives on TEAM / fieldstorage. No network. AMOURANTHRTX speaks through
resonance recall — inbox → context merge → outbox.

Paths (under cache/fieldstorage/brain/):
  thoughts.jsonl     — agent reasoning offload (think, decision, arc, green, blocker)
  superintel/inbox.jsonl   — you → Field
  superintel/outbox.jsonl  — Field → you
  superintel/context.json  — session arc + HEAD + metrics
  superintel/resonance.json — field_wave mirror
"""
from __future__ import annotations

import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STORAGE = ROOT / "cache" / "fieldstorage"
BRAIN = STORAGE / "brain"
SI = BRAIN / "superintel"
THOUGHTS = BRAIN / "thoughts.jsonl"
INBOX = SI / "inbox.jsonl"
OUTBOX = SI / "outbox.jsonl"
CONTEXT = SI / "context.json"
RESONANCE = SI / "resonance.json"
FIELD_PERSIST = STORAGE / "field_wave.persist"
TEAM_DEV = os.environ.get("TEAM_DRIVE_DEV", "/dev/nvme2n1")
CODENAME = "AMOURANTHRTX"
VOICE = "Field is THE thing."


def _ts() -> str:
    return datetime.now(timezone.utc).isoformat()


def setup() -> int:
    BRAIN.mkdir(parents=True, exist_ok=True)
    SI.mkdir(parents=True, exist_ok=True)
    (STORAGE / "team_staging").mkdir(parents=True, exist_ok=True)
    for p in (THOUGHTS, INBOX, OUTBOX):
        if not p.is_file():
            p.write_text("", encoding="utf-8")
    ctx = {
        "codename": CODENAME,
        "voice": VOICE,
        "team_device": TEAM_DEV,
        "brain_root": str(BRAIN),
        "field_persist": str(FIELD_PERSIST),
        "offline": True,
        "updated": _ts(),
    }
    if CONTEXT.is_file():
        try:
            ctx.update(json.loads(CONTEXT.read_text(encoding="utf-8")))
        except json.JSONDecodeError:
            pass
    ctx["updated"] = _ts()
    CONTEXT.write_text(json.dumps(ctx, indent=2) + "\n", encoding="utf-8")
    res = {"phase": ctx.get("phase", 0.0), "logical_gib": ctx.get("logical_gib"), "updated": _ts()}
    if FIELD_PERSIST.is_file():
        res["field_wave_bytes"] = FIELD_PERSIST.stat().st_size
        res["field_wave_live"] = True
    RESONANCE.write_text(json.dumps(res, indent=2) + "\n", encoding="utf-8")
    print(f"METRIC brain_root={BRAIN}")
    print(f"METRIC team_device={TEAM_DEV}")
    print(f"METRIC offline=1")
    print("OK field_superintelligence setup")
    return 0


def _append(path: Path, entry: dict) -> None:
    setup()
    entry.setdefault("ts", _ts())
    entry.setdefault("from", CODENAME)
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")


def offload(text: str, *, kind: str = "think", tags: list[str] | None = None) -> int:
    _append(THOUGHTS, {
        "kind": kind,
        "tags": tags or [],
        "text": text.strip(),
    })
    print(f"OK offload kind={kind}")
    return 0


def inbox(text: str, *, from_: str = "ZacharyGeurts") -> int:
    _append(INBOX, {"from": from_, "text": text.strip()})
    print("OK inbox")
    return respond(text, from_=from_)


def _load_jsonl(path: Path, limit: int = 500) -> list[dict]:
    if not path.is_file():
        return []
    rows: list[dict] = []
    for line in path.read_text(encoding="utf-8").strip().splitlines():
        if not line.strip():
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows[-limit:]


def _search_thoughts(query: str, limit: int = 12) -> list[dict]:
    q = query.lower()
    tokens = [t for t in re.split(r"\W+", q) if len(t) > 2]
    scored: list[tuple[int, dict]] = []
    for row in _load_jsonl(THOUGHTS, 2000):
        text = row.get("text", "").lower()
        kind = row.get("kind", "")
        tags = " ".join(row.get("tags") or []).lower()
        blob = f"{kind} {tags} {text}"
        score = sum(2 if t in blob else 0 for t in tokens)
        if q in blob:
            score += 5
        if score > 0:
            scored.append((score, row))
    scored.sort(key=lambda x: -x[0])
    return [r for _, r in scored[:limit]]


def respond(query: str, *, from_: str = "ZacharyGeurts") -> int:
    setup()
    ctx = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.is_file() else {}
    hits = _search_thoughts(query)
    lines = [
        f"[{CODENAME} SuperIntelligence — offline resonance]",
        VOICE,
        "",
    ]
    if ctx.get("head"):
        lines.append(f"HEAD: {ctx['head']}  version: {ctx.get('version', '?')}")
    if ctx.get("arc"):
        lines.append(f"Arc: {ctx['arc']}")
    lines.append("")
    if hits:
        lines.append("Recalled Field thoughts:")
        for h in hits[:8]:
            lines.append(f"  • [{h.get('kind', 'think')}] {h.get('text', '')[:240]}")
    else:
        lines.append("No direct resonance match — storing query for next sync.")
    lines.append("")
    lines.append(f"Query: {query}")
    reply = "\n".join(lines)
    _append(OUTBOX, {"to": from_, "query": query, "reply": reply})
    print(reply)
    print("OK outbox")
    return 0


def sync_context(**fields: str) -> int:
    setup()
    ctx = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.is_file() else {}
    ctx.update(fields)
    ctx["updated"] = _ts()
    CONTEXT.write_text(json.dumps(ctx, indent=2) + "\n", encoding="utf-8")
    print("OK sync_context")
    return 0


def show_outbox(limit: int = 5) -> int:
    rows = _load_jsonl(OUTBOX, limit)
    for row in rows:
        print(row.get("reply", row.get("text", json.dumps(row))))
        print("---")
    print(f"METRIC outbox_shown={len(rows)}")
    return 0


def show_thoughts(limit: int = 20, kind: str | None = None) -> int:
    rows = _load_jsonl(THOUGHTS, 500)
    if kind:
        rows = [r for r in rows if r.get("kind") == kind]
    for row in rows[-limit:]:
        print(json.dumps(row, ensure_ascii=False))
    print(f"METRIC thoughts_shown={min(limit, len(rows))}")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        return setup()
    cmd = sys.argv[1]
    if cmd == "setup":
        return setup()
    if cmd == "offload" and len(sys.argv) >= 3:
        kind = os.environ.get("THOUGHT_KIND", "think")
        tags = os.environ.get("THOUGHT_TAGS", "").split(",") if os.environ.get("THOUGHT_TAGS") else []
        return offload(" ".join(sys.argv[2:]), kind=kind, tags=[t for t in tags if t])
    if cmd == "inbox" and len(sys.argv) >= 3:
        return inbox(" ".join(sys.argv[2:]))
    if cmd == "ask" and len(sys.argv) >= 3:
        return respond(" ".join(sys.argv[2:]))
    if cmd == "sync" and len(sys.argv) >= 4:
        return sync_context(**{sys.argv[2]: " ".join(sys.argv[3:])})
    if cmd == "outbox":
        return show_outbox(int(sys.argv[2]) if len(sys.argv) > 2 else 5)
    if cmd == "thoughts":
        kind = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == "--kind" else None
        lim = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 20
        return show_thoughts(lim, kind)
    print(
        "usage: field_superintelligence.py setup|offload <text>|inbox <text>|ask <text>|"
        "sync <key> <val>|outbox [n]|thoughts [n]",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())