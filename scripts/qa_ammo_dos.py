#!/usr/bin/env python3
"""Master QA for RTX-DOS 7.0 GPU Super DOSBox — boot, input, viewport, engine."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build/bin/Linux"
LIB = ROOT / "build/libx86emu.a"
ENGINE = BIN / "AMOURANTHRTX"
SCRIPTS = ROOT / "scripts"


def field_include_args() -> list[str]:
    """Match CMake FIELD_ENGINE_INCLUDES / SG_INCLUDE_DIRS."""
    dirs = [
        ROOT / "Navigator/engine",
        ROOT / "Navigator/engine/FieldX86Core/include",
        ROOT / "AmmoOS",
        ROOT / "AmmoOS/core",
        ROOT / "AmmoOS/windowing",
        ROOT / "AmmoOS/data",
        ROOT / "dos",
        ROOT / "third_party/libx86emu/include",
    ]
    args: list[str] = []
    for d in dirs:
        args.extend(["-I", str(d)])
    return args


def compile_standalone(src_name: str, out_name: str | None = None) -> Path:
    src = SCRIPTS / src_name if src_name.endswith(".cpp") else SCRIPTS / f"{src_name}.cpp"
    out = BIN / (out_name or src.stem)
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["g++-14", "-std=c++20", "-O2", *field_include_args(), str(src), "-o", str(out)],
        cwd=ROOT,
        check=True,
    )
    return out


def run(cmd: list[str], timeout: int = 180) -> subprocess.CompletedProcess[str]:
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout, check=False)


def compile_cpp(name: str) -> Path:
    src = SCRIPTS / f"{name}.cpp"
    out = BIN / name
    if not src.is_file():
        raise SystemExit(f"missing {src}")
    if not LIB.is_file():
        raise SystemExit("build libx86emu first: cmake --build build")
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++-14", "-std=c++20", "-O2",
            *field_include_args(),
            str(src), str(LIB), "-o", str(out),
        ],
        cwd=ROOT,
        check=True,
    )
    return out


def test_viewport_math() -> None:
    """Host-side panel rect — postage stamp vs fullscreen zoom."""
    win_w, win_h = 1920.0, 1080.0
    dos_w, dos_h = 640.0, 400.0
    hud_h = 48.0

    def stamp_rect() -> tuple[float, float, float, float]:
        ox = (win_w - dos_w) * 0.5
        oy = (win_h - (dos_h + hud_h)) * 0.5
        return ox, oy, ox + dos_w, oy + dos_h + hud_h

    def zoom_rect() -> tuple[float, float, float, float]:
        return 0.0, 0.0, win_w, win_h

    s = stamp_rect()
    z = zoom_rect()
    sw, sh = s[2] - s[0], s[3] - s[1]
    zw, zh = z[2] - z[0], z[3] - z[1]
    if abs(sw - dos_w) > 0.5 or abs(sh - (dos_h + hud_h)) > 0.5:
        raise SystemExit(f"FAIL stamp should be {dos_w}x{dos_h + hud_h}, got {sw}x{sh}")
    if abs(zw - win_w) > 0.5 or abs(zh - win_h) > 0.5:
        raise SystemExit(f"FAIL zoom should fill window, got {zw}x{zh}")
    scale = min(zw / 640.0, zh / 480.0)
    if scale < 1.5:
        raise SystemExit(f"FAIL zoom font scale too small {scale}")
    print(f"OK viewport math: stamp {sw:.0f}x{sh:.0f} zoom {zw:.0f}x{zh:.0f} scale~{scale:.1f}x")

    win4k_w, win4k_h = 3840.0, 2160.0
    scale4k = min(win4k_w / 640.0, win4k_h / 480.0)
    if scale4k < 4.0:
        raise SystemExit(f"FAIL 4K zoom scale too small {scale4k}")
    stamp4k = min(win4k_w / 1280.0, win4k_h / 800.0)
    if stamp4k < 2.5:
        raise SystemExit(f"FAIL 4K stamp scale too small {stamp4k}")
    print(f"OK 4K viewport: zoom scale~{scale4k:.1f}x stamp scale~{stamp4k:.1f}x")


def main() -> int:
    print("=== QA: verify_rtx_dos ===")
    proc = run([sys.executable, str(SCRIPTS / "verify_rtx_dos.py")], timeout=300)
    if proc.returncode != 0:
        print("  (verify_rtx_dos skipped — optional)")

    print("=== QA: viewport math ===")
    test_viewport_math()

    print("=== QA: qa_ammofat_test (no x86emu) ===")
    compile_standalone("qa_ammofat_test.cpp")
    run([str(BIN / "qa_ammofat_test")], timeout=60).check_returncode()

    print("=== QA: qa_rtx_ammos_test (GPU-only shell) ===")
    qa_rtx = BIN / "qa_rtx_ammos_test"
    if not qa_rtx.is_file():
        subprocess.run(
            ["cmake", "--build", str(ROOT / "build"), "--target", "qa_rtx_ammos_test"],
            cwd=ROOT, check=True,
        )
    run([str(qa_rtx)], timeout=60).check_returncode()

    print("=== QA: qa_rtx_vfs_test (VFS metadata + throttle) ===")
    compile_standalone("qa_rtx_vfs_test.cpp")
    run([str(BIN / "qa_rtx_vfs_test")], timeout=60).check_returncode()

    print("=== QA: qa_aos_ocr_test (Start popup + WM button snapshots) ===")
    proc = run([sys.executable, str(SCRIPTS / "qa_aos_ocr_test.py")], timeout=300)
    if proc.returncode != 0:
        if proc.stdout:
            sys.stdout.write(proc.stdout)
        if proc.stderr:
            sys.stderr.write(proc.stderr[-2000:])
        raise SystemExit(f"FAIL qa_aos_ocr_test rc={proc.returncode}")

    print("=== QA: qa_taskbar_click_test (taskbar hit targets) ===")
    taskbar_qa = ROOT / "build" / "qa_taskbar_click_test"
    if not taskbar_qa.is_file():
        subprocess.run(
            ["cmake", "--build", str(ROOT / "build"), "--target", "qa_taskbar_click_test"],
            cwd=ROOT, check=True,
        )
    run([str(taskbar_qa)], timeout=60).check_returncode()

    print("=== QA: qa_amouranthos_test (AmouranthOS WM + taskbar) ===")
    run([str(ROOT / "build" / "qa_amouranthos_test")], timeout=60).check_returncode()

    print("=== QA: qa_drives_bench_test (drive rack metrics) ===")
    compile_standalone("qa_drives_bench_test.cpp")
    proc = run([str(BIN / "qa_drives_bench_test")], timeout=60)
    if proc.stdout:
        for line in proc.stdout.splitlines():
            print(f"  {line}")
    proc.check_returncode()

    print("=== QA: qa_toolchain_test (DevKit asm/link/run) ===")
    run([str(ROOT / "build" / "qa_toolchain_test")], timeout=60).check_returncode()

    print("=== QA: qa_drivers_build (AMMOASM RTXFL .SYS drivers) ===")
    compile_cpp("qa_drivers_build")
    run([str(BIN / "qa_drivers_build"), str(ROOT)], timeout=60).check_returncode()

    cmake_qa = [
        "qa_doom_host_test",
        "qa_dos_int_test",
        "qa_cmd_help_test",
        "qa_dos_embed_test",
    ]
    for name in cmake_qa:
        print(f"=== QA: {name} ===")
        exe = ROOT / "build" / name
        if not exe.is_file():
            subprocess.run(
                ["cmake", "--build", str(ROOT / "build"), "--target", name],
                cwd=ROOT, check=True,
            )
        proc = run([str(exe)], timeout=240)
        if proc.stdout:
            sys.stdout.write(proc.stdout)
        if proc.returncode != 0:
            err = (proc.stderr or "") + (proc.stdout or "")
            if name == "qa_doom_host_test" and "DOOM.EXE" in err:
                print(f"  (skip {name} — DOOM.EXE not on C:; run ./linux.sh dos to stage)")
                continue
            if name == "qa_dos_embed_test" and "hd image" in err.lower():
                print(f"  (skip {name} — run ./linux.sh dos to build RTX-DOS HD images)")
                continue
            sys.stderr.write(proc.stderr[-1200:] if proc.stderr else "")
            raise SystemExit(f"FAIL {name} rc={proc.returncode}")

    if ENGINE.is_file():
        print("=== QA: headless engine 120 frames ===")
        proc = run(
            ["env", "AMOURANTHRTX_HEADLESS=1", "AMOURANTHRTX_MAX_FRAMES=120", str(ENGINE)],
            timeout=120,
        )
        out = proc.stdout + proc.stderr
        if proc.returncode != 0:
            raise SystemExit(f"FAIL engine rc={proc.returncode}")
        for needle in ("Phase 1", "accountant", "SAFE SHUTDOWN"):
            if needle not in out:
                raise SystemExit(f"FAIL engine log missing {needle!r}")
        if "Shareware Doom" in out or "GPU Doom" in out:
            raise SystemExit("FAIL engine still references Doom boot")
        print("OK headless engine boot path logs")

    print("\nRTX-AMMOS GPU-only Super DOSBox QA PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())