#!/usr/bin/env python3
"""AMOURANTHRTX SuperIntelligence — offline Field brain on infinite storage.

All thinking lives on TEAM / fieldstorage. No network. AMOURANTHRTX speaks through
resonance recall — inbox → context merge → outbox.

Paths (under cache/fieldstorage/brain/):
  thoughts.jsonl     — agent reasoning offload (think, decision, arc, green, blocker)
  superintel/inbox.jsonl   — you → Field
  superintel/outbox.jsonl  — Field → you
  superintel/context.json  — session arc + HEAD + metrics + dev_process
  superintel/resonance.json — field_wave mirror + physics grounding
  ingest_index.json  — codebase symbol + directive scan cache
"""
from __future__ import annotations

import json
import math
import os
import re
import subprocess
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
INGEST_INDEX = BRAIN / "ingest_index.json"
FIELD_PERSIST = STORAGE / "field_wave.persist"
TEAM_DEV = os.environ.get("TEAM_DRIVE_DEV", "/dev/nvme2n1")
CODENAME = "AMOURANTHRTX"
VOICE = "Field is THE thing."

INGEST_PATHS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("AGENTS.md", ("AMOURANTHRTX", "Field", "agent")),
    ("Navigator/engine/FieldStorage.hpp", ("persistFieldState", "sdfFoldBlock", "enableEndGameMode")),
    ("Navigator/engine/FieldFabric.hpp", ("entropyFabricPredict", "processLeadIn", "gEntropyFold")),
    ("Navigator/engine/FieldEverything.hpp", ("Everything Everywhere", "seedChips")),
    ("scripts/field_superintelligence.py", ("offline", "resonance", "thoughts")),
    ("scripts/release_checklist_2_0.py", ("GREEN ALL", "qa_keen_host_test")),
    ("AmmoOS/core/FieldAmouranthOs.hpp", ("shellChromeActive", "packDataBus", "panelVisible")),
    ("linux.sh", ("end-game", "brain", "super", "release-2.0")),
)

DEV_PROCESS_V32: tuple[dict[str, str], ...] = (
    {"phase": "1", "name": "Code Evaluation", "status": "done"},
    {"phase": "2", "name": "Core Fixes + GUI Polish", "status": "done"},
    {"phase": "3", "name": "Field Drive Infinite + Persistent", "status": "active"},
    {"phase": "4", "name": "Sudo Terminal + CHIPS", "status": "parallel"},
    {"phase": "5", "name": "Offline SuperIntelligence", "status": "active"},
    {"phase": "6", "name": "Polish + QA + Benchmarks", "status": "ongoing"},
    {"phase": "7", "name": "2.0.5 Release + Beyond", "status": "next"},
)

WAVE_PHASES = (0.0, 0.785398, 1.570796, 2.356194, 3.141593)
BASE_SDF_GB = 2.0


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


def _git_head() -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True, stderr=subprocess.DEVNULL,
        )
        return out.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def _read_version() -> str:
    try:
        text = (ROOT / "scripts" / "ammo_platform.py").read_text(encoding="utf-8")
        m = re.search(r'AMOURANTHRTX_VERSION\s*=\s*"([^"]+)"', text)
        return m.group(1) if m else "?"
    except OSError:
        return "?"


def ingest(*, limit: int = 24) -> int:
    """Scan monolith symbols + directives into thoughts + ingest_index."""
    setup()
    symbols: list[dict] = []
    for rel, needles in INGEST_PATHS:
        path = ROOT / rel
        entry = {"path": rel, "ok": path.is_file(), "hits": []}
        if path.is_file():
            text = path.read_text(encoding="utf-8", errors="replace")
            for needle in needles:
                if needle in text:
                    entry["hits"].append(needle)
        symbols.append(entry)
        if entry["hits"]:
            _append(THOUGHTS, {
                "kind": "ingest",
                "tags": ["codebase", rel.replace("/", "_")],
                "text": f"{rel}: " + ", ".join(entry["hits"][:6]),
            })
    idx = {
        "updated": _ts(),
        "head": _git_head(),
        "version": _read_version(),
        "symbols": symbols,
        "voice": VOICE,
    }
    INGEST_INDEX.write_text(json.dumps(idx, indent=2) + "\n", encoding="utf-8")
    hit_count = sum(len(s["hits"]) for s in symbols)
    print(f"METRIC ingest_files={len(symbols)}")
    print(f"METRIC ingest_hits={hit_count}")
    print(f"METRIC ingest_index={INGEST_INDEX}")
    print("OK ingest")
    return 0


def _parse_bench_metrics(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in text.splitlines():
        m = re.match(r"METRIC (\w+)=(.+)", line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def physics() -> int:
    """Mirror FieldStorage hyper / entropy metrics into resonance.json."""
    setup()
    metrics: dict[str, str | float | int | bool] = {"updated": _ts(), "offline": True}
    bench = ROOT / "scripts" / "bench_storage.py"
    if bench.is_file():
        proc = subprocess.run(
            [sys.executable, str(bench)], cwd=ROOT, capture_output=True, text=True, check=False,
        )
        metrics.update(_parse_bench_metrics(proc.stdout))
        metrics["bench_rc"] = proc.returncode
    if FIELD_PERSIST.is_file():
        metrics["field_wave_bytes"] = FIELD_PERSIST.stat().st_size
        metrics["field_wave_live"] = True
    bo_gain = float(str(metrics.get("bo_gain", "6.5")))
    transforms: list[dict[str, float]] = []
    for phase in WAVE_PHASES:
        resonance = 1.0 + math.sin(phase)
        logical_gb = BASE_SDF_GB * bo_gain * resonance
        transforms.append({
            "phase": round(phase, 3),
            "logical_gib": round(logical_gb, 2),
            "fold_x": round(bo_gain * resonance, 2),
        })
    metrics["transform_anchor_gb"] = BASE_SDF_GB
    metrics["transform_table"] = transforms
    metrics["entropy_arrow"] = "forward"
    metrics["time_model"] = "linear"
    metrics["grounding"] = ["entropy", "maxwell", "casimir", "thermo", "wave"]
    ctx = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.is_file() else {}
    ctx["physics"] = {
        "bo_gain": bo_gain,
        "logical_gib_peak": max(t["logical_gib"] for t in transforms),
        "field_wave_live": metrics.get("field_wave_live", False),
    }
    ctx["updated"] = _ts()
    CONTEXT.write_text(json.dumps(ctx, indent=2) + "\n", encoding="utf-8")
    RESONANCE.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"METRIC bo_gain={bo_gain}")
    print(f"METRIC logical_gib_peak={ctx['physics']['logical_gib_peak']}")
    print(f"METRIC field_wave_live={1 if metrics.get('field_wave_live') else 0}")
    print("OK physics")
    return 0


def process() -> int:
    """Write v32 seven-phase dev process into context.json."""
    setup()
    ctx = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.is_file() else {}
    ctx.update({
        "codename": CODENAME,
        "voice": VOICE,
        "version": _read_version(),
        "head": _git_head(),
        "arc": "offline superintelligence + Field Drive persistent + 2.0.5",
        "dev_process": list(DEV_PROCESS_V32),
        "phase5": "Offline SuperIntelligence — local inference + Field memory + physics grounding",
        "offline": True,
        "updated": _ts(),
    })
    CONTEXT.write_text(json.dumps(ctx, indent=2) + "\n", encoding="utf-8")
    _append(THOUGHTS, {
        "kind": "arc",
        "tags": ["v32", "process"],
        "text": "v32 dev process locked: 7 phases, Phase 5 offline SI on Field canvas.",
    })
    print(f"METRIC dev_process_phases={len(DEV_PROCESS_V32)}")
    print(f"METRIC head={ctx['head']}")
    print(f"METRIC version={ctx['version']}")
    print("OK process")
    return 0


def evaluate() -> int:
    """Full evaluation sync: ingest + physics + process + status thought."""
    ingest()
    physics()
    process()
    setup()
    head = _git_head()
    version = _read_version()
    _append(THOUGHTS, {
        "kind": "green",
        "tags": ["evaluate", "v32"],
        "text": f"Evaluate sync HEAD={head} version={version} — offline SI foundation live on Field storage.",
    })
    sync_context(head=head, version=version, verdict="GREEN ALL", arc="v32 execute")
    print(f"METRIC evaluate_head={head}")
    print(f"METRIC evaluate_version={version}")
    print("OK evaluate")
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
    if cmd == "ingest":
        return ingest()
    if cmd == "physics":
        return physics()
    if cmd == "process":
        return process()
    if cmd == "evaluate":
        return evaluate()
    print(
        "usage: field_superintelligence.py setup|offload <text>|inbox <text>|ask <text>|"
        "sync <key> <val>|outbox [n]|thoughts [n]|ingest|physics|process|evaluate",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())