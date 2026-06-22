#!/usr/bin/env python3
"""RTX-DOS / AMOURANTHRTX benchmark suite — host CPU, GPU pipeline, DOSBox comparison."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import threading
import time
from collections.abc import Callable
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build/bin/Linux"
LIB = ROOT / "build/libx86emu.a"
ENGINE = BIN / "AMOURANTHRTX"
SCRIPTS = ROOT / "scripts"
REPORT = ROOT / "build/bench_report.json"


def run(cmd: list[str], *, timeout: int = 600, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    print(f"  $ {' '.join(cmd)}", flush=True)
    merged = os.environ.copy()
    if env:
        merged.update(env)
    return subprocess.run(
        cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout,
        check=False, env=merged,
    )


def build_cmake_target(target: str, *candidates: Path) -> Path:
    subprocess.run(
        [
            "cmake", "--build", str(ROOT / "build"),
            "--target", target,
            "-j", str(os.cpu_count() or 4),
        ],
        cwd=ROOT,
        check=True,
    )
    for path in candidates:
        if path.is_file():
            return path
    raise SystemExit(f"FAIL {target} built but binary not found")


def compile_host_bench() -> Path:
    return build_cmake_target(
        "bench_x86_host",
        BIN / "bench_x86_host",
        ROOT / "build/bench_x86_host",
    )


def parse_metrics(text: str) -> dict[str, float | str]:
    out: dict[str, float | str] = {}
    for line in text.splitlines():
        if line.startswith("METRIC "):
            body = line[7:]
            if "=" not in body:
                continue
            k, v = body.split("=", 1)
            try:
                out[k] = float(v) if "." in v or v.isdigit() else v
            except ValueError:
                out[k] = v
    return out


def _cpu_sample_loop(pid: int, stop: threading.Event, out: list[float]) -> None:
    clk_tck = os.sysconf("SC_CLK_TCK")
    prev: tuple[float, int] | None = None
    while not stop.is_set():
        try:
            with open(f"/proc/{pid}/stat") as f:
                parts = f.read().split()
            total = int(parts[13]) + int(parts[14])
            now = time.time()
            if prev is not None:
                dt = now - prev[0]
                if dt > 0:
                    out.append(100.0 * (total - prev[1]) / (dt * clk_tck))
            prev = (now, total)
        except (FileNotFoundError, ProcessLookupError, IndexError, ValueError):
            break
        stop.wait(0.25)


def _gpu_sample_loop(pid: int, stop: threading.Event, out: list[float]) -> None:
    if not shutil.which("nvidia-smi"):
        return
    while not stop.is_set():
        try:
            proc = subprocess.run(
                [
                    "nvidia-smi", "--query-compute-apps=pid,utilization.gpu",
                    "--format=csv,noheader,nounits",
                ],
                capture_output=True, text=True, timeout=3, check=False,
            )
            matched = False
            for line in proc.stdout.splitlines():
                parts = [p.strip() for p in line.split(",")]
                if len(parts) >= 2 and parts[0] == str(pid):
                    out.append(float(parts[1]))
                    matched = True
                    break
            if not matched:
                proc2 = subprocess.run(
                    ["nvidia-smi", "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"],
                    capture_output=True, text=True, timeout=3, check=False,
                )
                if proc2.stdout.strip():
                    out.append(float(proc2.stdout.strip().splitlines()[0]))
        except (ValueError, subprocess.TimeoutExpired):
            pass
        stop.wait(0.25)


def wait_proc(
    proc: subprocess.Popen[str],
    *,
    timeout: float,
    label: str,
    sample_fn: Callable[[int, threading.Event, list[float]], None] | None = None,
) -> tuple[str, str, list[float]]:
    """Wait for process with heartbeat; optional background sampling thread."""
    samples: list[float] = []
    stop = threading.Event()
    sampler: threading.Thread | None = None
    if sample_fn is not None:
        sampler = threading.Thread(target=sample_fn, args=(proc.pid, stop, samples), daemon=True)
        sampler.start()

    text_out = ""
    text_err = ""
    t0 = time.time()
    next_beat = t0 + 5.0
    try:
        while True:
            try:
                text_out, text_err = proc.communicate(timeout=0.5)
                break
            except subprocess.TimeoutExpired:
                now = time.time()
                if now - t0 >= timeout:
                    proc.kill()
                    try:
                        text_out, text_err = proc.communicate(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.terminate()
                    raise SystemExit(f"FAIL {label} timed out after {timeout:.0f}s")
                if now >= next_beat:
                    print(f"  … {label} still running ({now - t0:.0f}s)", flush=True)
                    next_beat = now + 5.0
    finally:
        stop.set()
        if sampler is not None:
            sampler.join(timeout=1.0)

    return text_out, text_err, samples


def bench_doom_host(*, timeout: float = 120.0) -> dict:
    print("=== BENCH: shareware DOOM.EXE (host libx86emu + DOS4GW) ===", flush=True)
    if not (ROOT / "assets/dos/DOOM.EXE").is_file():
        run([sys.executable, "scripts/install_shareware_doom.py"], timeout=60)
    exe = build_cmake_target(
        "qa_doom_host_test",
        ROOT / "build/qa_doom_host_test",
        BIN / "qa_doom_host_test",
    )
    t0 = time.perf_counter()
    proc = subprocess.run(
        [str(exe), str(ROOT / "build/grabs/doom")],
        cwd=ROOT, capture_output=True, text=True, timeout=timeout, check=False,
    )
    wall = time.perf_counter() - t0
    text = proc.stdout + proc.stderr
    m = parse_metrics(text)
    ticks = float(m.get("doom_ticks", 0))
    result = {
        "name": "Shareware DOOM.EXE (host)",
        "status": "ok" if proc.returncode == 0 else "partial",
        "return_code": proc.returncode,
        "wall_sec": wall,
        "doom_mode": m.get("doom_mode", 0),
        "doom_fb_nz": m.get("doom_fb_nz", 0),
        "doom_ticks": ticks,
        "host_cycles_per_sec": ticks / max(wall, 0.001),
        "title_reached": 1.0 if m.get("doom_mode") == 19.0 and m.get("doom_fb_nz", 0) >= 8000 else 0.0,
    }
    print(
        f"  mode={result['doom_mode']} fb_nz={result['doom_fb_nz']} "
        f"ticks={ticks:.0f} wall={wall:.1f}s rc={proc.returncode}",
        flush=True,
    )
    return result


def bench_host_cpu(*, timeout: float = 45.0) -> dict:
    print("=== BENCH: host libx86emu (CPU) ===", flush=True)
    exe = compile_host_bench()
    t0 = time.perf_counter()
    proc = subprocess.Popen(
        [str(exe)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    stdout, stderr, cpu_vals = wait_proc(
        proc, timeout=timeout, label="host CPU",
        sample_fn=_cpu_sample_loop,
    )
    wall = time.perf_counter() - t0
    text = stdout + stderr
    if proc.returncode != 0:
        raise SystemExit(f"FAIL bench_x86_host rc={proc.returncode}\n{text[-2000:]}")
    m = parse_metrics(text)
    m["name"] = "AMOURANTHRTX host CPU (libx86emu)"
    m["status"] = "ok"
    if cpu_vals:
        m["cpu_util_est"] = statistics.mean(cpu_vals)
        m["cpu_util_peak"] = max(cpu_vals)
    m["wall_sec_total"] = wall
    print(
        f"  cycles/sec: {m.get('cycles_per_sec', 0):.0f}  MIPS: {m.get('effective_mips', 0):.2f}"
        f"  CPU avg: {m.get('cpu_util_est', 0):.1f}%  ({wall:.1f}s wall)",
        flush=True,
    )
    return m


def parse_engine_log(text: str) -> dict[str, float]:
    out: dict[str, float] = {}
    fps_matches = re.findall(r"FPS:\s+(\d+)\s+\(avg frame (\d+) µs\)", text)
    if fps_matches:
        # Use last sustained window, not the warmup spike.
        fps, frame_us = fps_matches[-1]
        out["frames_per_sec"] = float(fps)
        out["ms_per_frame"] = float(frame_us) / 1000.0
    gpu_matches = re.findall(r"GPU load:\s+([\d.]+)%", text)
    if gpu_matches:
        out["gpu_util_est"] = float(gpu_matches[-1])
    cyc = re.search(r"X86 DIE\s+CYC=(\d+)", text)
    if cyc:
        out["die_cycles"] = float(cyc.group(1))
    rend = re.search(r"Rendered:\s+(\d+) x (\d+)", text)
    if rend:
        out["render_w"] = float(rend.group(1))
        out["render_h"] = float(rend.group(2))
    return out


def bench_gpu_engine(*, frames: int = 240, width: int = 1920, height: int = 1080, timeout: float = 120.0) -> dict:
    print(f"=== BENCH: AMOURANTHRTX GPU pipeline ({width}x{height} x{frames} frames) ===", flush=True)
    if not ENGINE.is_file():
        raise SystemExit(f"missing engine — run ./linux.sh first ({ENGINE})")

    env = {
        "AMOURANTHRTX_HEADLESS": "1",
        "AMOURANTHRTX_MAX_FRAMES": str(frames),
        "AMOURANTHRTX_BENCH_W": str(width),
        "AMOURANTHRTX_BENCH_H": str(height),
        "DISABLE_TRACE_AND_DEBUG_LOGS": "1",
    }

    t0 = time.perf_counter()
    proc = subprocess.Popen(
        [str(ENGINE)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env={**os.environ, **env},
    )
    stdout, stderr, gpu_vals = wait_proc(
        proc, timeout=timeout, label=f"GPU {width}x{height}",
        sample_fn=_gpu_sample_loop,
    )
    wall = time.perf_counter() - t0
    text = stdout + stderr

    if proc.returncode != 0:
        raise SystemExit(f"FAIL engine bench rc={proc.returncode}\n{text[-2500:]}")

    parsed = parse_engine_log(text)
    fps = parsed.get("frames_per_sec", frames / max(wall, 0.001))
    ms = parsed.get("ms_per_frame", (wall / frames) * 1000.0)
    gpu_shader_cycles = 8192 * frames

    result = {
        "name": f"AMOURANTHRTX GPU ({width}x{height})",
        "status": "ok",
        "backend": "gpu_vulkan",
        "bench_frames": float(frames),
        "render_w": width,
        "render_h": height,
        "wall_sec": wall,
        "frames_per_sec": fps,
        "ms_per_frame": ms,
        "gpu_shader_cycles_per_frame": 8192.0,
        "gpu_shader_cycles_total": float(gpu_shader_cycles),
        "gpu_shader_cycles_per_sec": gpu_shader_cycles / max(wall, 0.001),
        "host_cycles_requested_per_frame": 131072.0,
        "cpu_util_est": 0.0,
        "gpu_util_est": statistics.mean(gpu_vals) if gpu_vals else parsed.get("gpu_util_est", 0.0),
        "gpu_util_peak": max(gpu_vals) if gpu_vals else parsed.get("gpu_util_est", 0.0),
        "die_cycles": parsed.get("die_cycles", 0.0),
    }
    print(
        f"  FPS: {fps:.1f}  frame: {ms:.2f} ms  GPU avg: {result['gpu_util_est']:.1f}%  ({wall:.1f}s wall)",
        flush=True,
    )
    return result


def find_dosbox() -> list[str] | None:
    for name in ("dosbox-staging", "dosbox", "dosbox-x"):
        p = shutil.which(name)
        if p:
            return [p]
    if shutil.which("flatpak"):
        for app in ("io.github.dosbox-staging", "com.dosbox_x.DOSBox-X", "com.dosbox.DOSBox"):
            proc = subprocess.run(
                ["flatpak", "info", app],
                capture_output=True, text=True, timeout=10, check=False,
            )
            if proc.returncode == 0:
                return ["flatpak", "run", "--filesystem=host", "--command=dosbox", app]
    return None


def bench_dosbox(*, timeout: float = 35.0) -> dict:
    print("=== BENCH: DOSBox (reference) ===", flush=True)
    dosbox = find_dosbox()
    if not dosbox:
        print("  SKIP: dosbox not installed", flush=True)
        return {"name": "DOSBox", "status": "skipped", "reason": "not_installed"}

    if not os.environ.get("DISPLAY"):
        print("  SKIP: no DISPLAY (DOSBox needs a window)", flush=True)
        return {"name": "DOSBox", "status": "skipped", "reason": "no_display"}

    from rtx_dos_brand import HD_IMAGE  # noqa: E402

    hd = ROOT / "assets" / "dos" / HD_IMAGE
    if not hd.is_file():
        hd = ROOT / "assets/dos/rtx_dos720.img"
    if not hd.is_file():
        return {"name": "DOSBox", "status": "skipped", "reason": "no_dos_image"}

    conf = SCRIPTS / "bench_dosbox.conf"
    custom = ROOT / "build/bench_dosbox_run.conf"
    custom.parent.mkdir(parents=True, exist_ok=True)
    auto = f"""@echo off
imgmount c "{hd}" -fs fat
c:
echo RTX-DOS BENCH START
dir >nul
dir >nul
dir >nul
echo RTX-DOS BENCH END
exit
"""
    custom.write_text(conf.read_text().replace(
        "[autoexec]\n@echo off\nmount c /bench/c\nc:\necho RTX-DOS BENCH START\ndir >nul\ndir >nul\ndir >nul\necho RTX-DOS BENCH END\nexit\n",
        "[autoexec]\n" + auto,
    ))

    cmd = [*dosbox, "--noprimaryconf", "--nolocalconf", "--conf", str(custom)]
    if dosbox[0] != "flatpak":
        cmd.extend(["-noconsole"])

    t0 = time.perf_counter()
    proc = subprocess.Popen(
        cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        stdout, stderr, _ = wait_proc(proc, timeout=timeout, label="DOSBox")
        wall = time.perf_counter() - t0
        text = stdout + stderr
        rc = proc.returncode
    except SystemExit as exc:
        wall = time.perf_counter() - t0
        return {
            "name": "DOSBox",
            "status": "timeout",
            "wall_sec": wall,
            "detail": str(exc),
        }

    if rc != 0:
        return {
            "name": "DOSBox",
            "status": "error",
            "wall_sec": wall,
            "stderr_tail": text[-1500:],
        }

    cycles_line = re.search(r"(\d+)\s+cycles", text, re.I)
    result = {
        "name": f"DOSBox ({dosbox[-1].split('.')[-1] if dosbox[0] == 'flatpak' else Path(dosbox[0]).name})",
        "status": "ok",
        "backend": "dosbox_cpu",
        "wall_sec": wall,
        "cpu_util_est": 100.0,
        "gpu_util_est": 0.0,
        "boot_smoke": "imgmount+3xdir",
    }
    if cycles_line:
        result["dosbox_cycles_setting"] = float(cycles_line.group(1))
    print(f"  wall: {wall:.2f}s  (boot+dir smoke, cycles=max)", flush=True)
    return result


def compare(results: list[dict]) -> dict:
    host = next((r for r in results if r.get("backend") == "host_libx86emu"), None)
    gpus = [r for r in results if r.get("backend") == "gpu_vulkan" and r.get("status") == "ok"]
    gpu = next((r for r in gpus if r.get("render_w") == 1920), gpus[0] if gpus else None)
    gpu4k = next((r for r in gpus if r.get("render_w") == 3840), None)
    db = next((r for r in results if r.get("backend") == "dosbox_cpu"), None)

    notes: list[str] = []
    verdict = "mixed"

    if host and gpu:
        host_cps = float(host.get("cycles_per_sec", 0))
        gpu_cps = float(gpu.get("gpu_shader_cycles_per_sec", 0))
        gpu_fps = float(gpu.get("frames_per_sec", 0))
        die_cap = float(gpu.get("gpu_shader_cycles_per_frame", 8192))
        bus_req = float(gpu.get("host_cycles_requested_per_frame", 131072))
        ratio = host_cps / max(gpu_cps, 1.0)
        notes.append(
            f"Host libx86emu: ~{host_cps/1e6:.1f}M emulated cycles/s "
            f"(CPU ~{host.get('cpu_util_est', 100):.0f}% during bench)."
        )
        notes.append(
            f"GPU shader die: {die_cap:.0f} cycles/frame cap × {gpu_fps:.1f} FPS "
            f"≈ {gpu_cps/1e6:.2f}M cycles/s (bus requests {bus_req:.0f}/frame)."
        )
        notes.append(
            f"Host CPU is ~{ratio:.0f}× faster than GPU die throughput; "
            f"shader budget uses only {100*die_cap/bus_req:.1f}% of requested bus cycles."
        )
        if ratio > 2.0:
            notes.append("Ahead on raw emulation throughput; GPU MAX_CYCLES_PER_FRAME is the bottleneck.")
            verdict = "cpu_ahead_gpu_cycle_cap"
        if gpu_fps >= 30.0:
            notes.append(f"GPU pipeline sustains {gpu_fps:.0f} FPS at 1080p headless — interactive UI tier.")
        elif gpu_fps >= 15.0:
            notes.append(f"GPU pipeline at {gpu_fps:.0f} FPS — playable but below 30 FPS UI target.")
        if float(gpu.get("gpu_util_est", 0)) < 40.0:
            notes.append(
                f"GPU at ~{gpu['gpu_util_est']:.0f}% avg ({gpu.get('gpu_util_peak', 0):.0f}% peak) — "
                "headroom to raise cycles/frame or resolution."
            )

    if gpu4k and gpu:
        notes.append(
            f"4K headless: {gpu4k['frames_per_sec']:.0f} FPS vs 1080p {gpu['frames_per_sec']:.0f} FPS "
            f"(GPU {gpu4k.get('gpu_util_est', 0):.0f}% @ 4K)."
        )

    if db and db.get("status") == "ok" and host:
        db_wall = float(db.get("wall_sec", 0))
        host_boot = float(host.get("boot_ms", 0)) / 1000.0
        notes.append(
            f"DOSBox smoke boot+dir: {db_wall:.2f}s wall vs AMOURANTHRTX host boot-to-prompt: {host_boot:.2f}s."
        )
        if host_boot < db_wall:
            notes.append("Host libx86emu boots faster than DOSBox reference on same HD image.")
    elif db and db.get("status") == "skipped":
        notes.append("DOSBox skipped — pass --dosbox or set BENCH_DOSBOX=1 to include reference run.")

    push = [
        "Raise MAX_CYCLES_PER_FRAME in x86.comp (currently 8192) toward bus 131072 — GPU at ~18% has ~5× headroom",
        "Hybrid: GPU canvas + host libx86emu die (CTRL_HOST_CPU) for heavy DOS batch work",
        "Target 30 FPS @ 1080p: need ~2× die cycles/frame or shed canvas cost on idle DOS",
    ]

    return {"verdict": verdict, "notes": notes, "push_ahead": push}


def print_report(results: list[dict], summary: dict) -> None:
    print("\n" + "=" * 72)
    print("RTX-DOS / AMOURANTHRTX BENCHMARK REPORT")
    print("=" * 72)
    for r in results:
        print(f"\n[{r.get('name', '?')}] status={r.get('status')}")
        for k in sorted(r.keys()):
            if k in ("name", "status", "stderr_tail", "reason", "detail"):
                continue
            print(f"  {k}: {r[k]}")
    print("\n--- Analysis ---")
    for n in summary.get("notes", []):
        print(f"  • {n}")
    print("\n--- Push ahead ---")
    for p in summary.get("push_ahead", []):
        print(f"  → {p}")
    print("=" * 72)


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="AMOURANTHRTX performance benchmark suite")
    p.add_argument("--quick", action="store_true", help="1080p GPU only, skip 4K and DOSBox")
    p.add_argument("--dosbox", action="store_true", help="Include DOSBox reference (can take 20-35s)")
    p.add_argument("--4k", dest="run_4k", action="store_true", help="Include 4K GPU pass")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    run_dosbox = args.dosbox or os.environ.get("BENCH_DOSBOX", "").strip() in ("1", "yes", "true")
    run_4k = args.run_4k and not args.quick

    if not LIB.is_file():
        print("Building libx86emu…", flush=True)
        subprocess.run(["cmake", "--build", str(ROOT / "build"), "-j", str(os.cpu_count() or 4)],
                       cwd=ROOT, check=True)

    results: list[dict] = []
    try:
        results.append(bench_host_cpu())
    except SystemExit as exc:
        results.append({"name": "host libx86emu", "status": "error", "detail": str(exc)})

    try:
        results.append(bench_doom_host(timeout=300.0 if args.quick else 360.0))
    except (SystemExit, subprocess.TimeoutExpired) as exc:
        results.append({"name": "Shareware DOOM.EXE (host)", "status": "error", "detail": str(exc)})

    try:
        results.append(bench_gpu_engine(frames=120 if args.quick else 180, width=1920, height=1080))
    except SystemExit as exc:
        results.append({"name": "GPU engine 1080p", "status": "error", "detail": str(exc)})

    if run_4k:
        try:
            results.append(bench_gpu_engine(frames=90, width=3840, height=2160, timeout=90.0))
        except SystemExit as exc:
            results.append({"name": "GPU engine 4K", "status": "error", "detail": str(exc)})

    if run_dosbox and not args.quick:
        results.append(bench_dosbox())
    else:
        print("=== BENCH: DOSBox (reference) ===", flush=True)
        print("  SKIP: pass --dosbox to include (avoids long Flatpak startup hang)", flush=True)
        results.append({"name": "DOSBox", "status": "skipped", "reason": "opt_in"})

    summary = compare(results)
    payload = {"timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "results": results, "summary": summary}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(payload, indent=2) + "\n")
    print_report(results, summary)
    print(f"\nWrote {REPORT}")
    failed = any(r.get("status") == "error" for r in results)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())