#!/usr/bin/env python3
"""RTX-AMMOS DevKit v4 — TOOLS/, AMMOINC/, SAMPLES/ (host shell runs real toolchain)."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "assets" / "dos" / "ammo")
    tools = out / "TOOLS"
    inc = out / "AMMOINC"
    samples = out / "SAMPLES"
    tools.mkdir(parents=True, exist_ok=True)
    inc.mkdir(parents=True, exist_ok=True)
    samples.mkdir(parents=True, exist_ok=True)

    mk_tools = ROOT / "scripts" / "mk_tools_com.py"
    if mk_tools.is_file():
        subprocess.run([sys.executable, str(mk_tools)], check=True, cwd=ROOT)

    (inc / "RTX.INC").write_text(
        "; RTX-AMMOS AMMOINC v4 - MASM intrinsics\r\n"
        "RTX_PUTS MACRO msg\r\n"
        "    mov ah,9\r\n"
        "    mov dx,offset msg\r\n"
        "    int 21h\r\n"
        "ENDM\r\n"
        "RTX_OUT MACRO ch\r\n"
        "    mov ah,2\r\n"
        "    mov dl,ch\r\n"
        "    int 21h\r\n"
        "ENDM\r\n"
        "RTX_EXIT MACRO code:=0\r\n"
        "    mov ax,4C00h + code\r\n"
        "    int 21h\r\n"
        "ENDM\r\n",
        encoding="ascii",
    )
    (inc / "RTX.H").write_text(
        "/* RTX-AMMOS C intrinsics - AMMOCC v4 subset */\r\n"
        "void rtx_puts(const char* s);\r\n"
        "void rtx_out(int ch);\r\n"
        "#define RTX_VERSION \"4.0.2026\"\r\n"
        "#define TESLA_R_FWD_MILLI 180\r\n"
        "#define TESLA_R_REV_MILLI 3200\r\n"
        "#define FIELD_BUS_TESLA 31\r\n",
        encoding="ascii",
    )
    (inc / "FIELD.H").write_text(
        "/* RTX Field theory absolutes - AMMOINC/FIELD.H */\r\n"
        "#define FIELD_THEORY_REV \"2026.1\"\r\n"
        "#define FIELD_LAYER_RAM 0\r\n"
        "#define FIELD_LAYER_VGA 1\r\n"
        "#define FIELD_LAYER_FAT 2\r\n"
        "#define FIELD_LAYER_COUNT 10\r\n"
        "#define FIELD_BUS_TESLA 31\r\n"
        "#define TESLA_R_FWD_MILLI 180\r\n"
        "#define TESLA_R_REV_MILLI 3200\r\n"
        "#define FIELD_PHI_MILLI 618\r\n"
        "#define FIELD_RAM_BYTES 0x04000000u\r\n"
        "#define FIELD_BOOT_VECTOR 0x7C00u\r\n",
        encoding="ascii",
    )
    (inc / "FIELD.INC").write_text(
        "; FIELD.INC - Tesla valve + layer absolutes (AMMOASM)\r\n"
        "TESLA_R_FWD_MILLI EQU 180\r\n"
        "TESLA_R_REV_MILLI EQU 3200\r\n"
        "FIELD_BUS_TESLA   EQU 31\r\n"
        "LAYER_FAT         EQU 2\r\n"
        "RTX_MUX_AH        EQU 052h\r\n",
        encoding="ascii",
    )
    (samples / "HELLO.ASM").write_text(
        ".MODEL TINY\n"
        ".CODE\n"
        "ORG 100h\n"
        "start:\n"
        "    mov ah,9\n"
        "    mov dx,offset msg\n"
        "    int 21h\n"
        "    mov ax,4C00h\n"
        "    int 21h\n"
        ".DATA\n"
        "msg db 'Hello RTX-DOS 7.0',13,10,'$'\n"
        "END start\n",
        encoding="ascii",
    )
    (samples / "HELLO.C").write_text(
        "/* RTX-DOS 7.0 sample - AMMOCC v4 */\r\n"
        "void main() {\r\n"
        "    int n;\r\n"
        "    n = 3;\r\n"
        "    rtx_puts(\"AMMOCC v4 on RTX-DOS 7.0\\r\\n\");\r\n"
        "    if (n < 5) rtx_puts(\"ok\\r\\n\");\r\n"
        "    return 0;\r\n"
        "}\r\n",
        encoding="ascii",
    )
    (samples / "PADTEST.FLD").write_text(
        "field PADTEST\r\n"
        "print \"Run PADTEST for Xbox360 live view\"\r\n"
        "return 0\r\n",
        encoding="ascii",
    )
    (samples / "UTIL.ASM").write_text(
        ".MODEL TINY\n.CODE\nPUBLIC RTXPUT\nRTXPUT:\n"
        "    mov ah,2\n    mov dl,'*'\n    int 21h\n    ret\nEND\n",
        encoding="ascii",
    )
    (samples / "MAIN.ASM").write_text(
        ".MODEL TINY\n.CODE\nORG 100h\nEXTRN RTXPUT\nstart:\n"
        "    call RTXPUT\n    mov ah,9\n    mov dx,offset msg\n    int 21h\n"
        "    mov ax,4C00h\n    int 21h\n.DATA\n"
        "msg db ' MULTI v4',13,10,'$'\nEND start\n",
        encoding="ascii",
    )
    (samples / "DOOMBOOT.ASM").write_text(
        "INCLUDE FIELD.INC\n"
        ".MODEL TINY\n.CODE\nORG 100h\nstart:\n"
        "    mov ah,9\n    mov dx,offset msg\n    int 21h\n"
        "    mov ax,TESLA_R_FWD_MILLI\n"
        "    cmp ax,180\n"
        "    je ok\n"
        "    mov ah,9\n    mov dx,offset bad\n    int 21h\n"
        "    mov ax,4C01h\n    int 21h\n"
        "ok:\n"
        "    mov ah,9\n    mov dx,offset go\n    int 21h\n"
        "    mov ax,4C00h\n    int 21h\n"
        ".DATA\n"
        "msg db 'DOOMBOOT RTX-DOS 7.0 Field Die',13,10,'$'\n"
        "bad db 'Tesla valve mismatch',13,10,'$'\n"
        "go  db 'Launch GAMES/DOOM/DOOM.EXE',13,10,'$'\n"
        "END start\n",
        encoding="ascii",
    )
    (samples / "DOOMBOOT.C").write_text(
        "/* DOOMBOOT - AMMOCC launcher with field absolutes */\r\n"
        "#include \"FIELD.H\"\r\n"
        "#include \"RTX.H\"\r\n"
        "void main() {\r\n"
        "    int fwd;\r\n"
        "    int rev;\r\n"
        "    fwd = TESLA_R_FWD_MILLI;\r\n"
        "    rev = TESLA_R_REV_MILLI;\r\n"
        "    rtx_puts(\"DOOMBOOT AMMOCC Field Die\\r\\n\");\r\n"
        "    if (fwd < rev) rtx_puts(\"Tesla valve OK\\r\\n\");\r\n"
        "    rtx_puts(\"Run GAMES/DOOM/DOOM.EXE\\r\\n\");\r\n"
        "    return 0;\r\n"
        "}\r\n",
        encoding="ascii",
    )
    (samples / "BUILD.DOOM").write_text(
        "@ECHO OFF\r\n"
        "REM RTX-DOS 7.0 one-shot DOOMBOOT build\r\n"
        "BUILD DOOMBOOT\r\n"
        "IF ERRORLEVEL 1 GOTO FAIL\r\n"
        "AMMORUN DOOMBOOT.COM\r\n"
        "GOTO END\r\n"
        ":FAIL\r\n"
        "ECHO BUILD DOOMBOOT failed\r\n"
        ":END\r\n",
        encoding="ascii",
    )
    (tools / "README.TXT").write_text(
        "RTX-AMMOS DevKit v4.0.2026\r\n"
        "Host GPU shell runs AMMOASM/AMMOSYS/AMMOCC/AMMOLINK/AMMODECOMP/AMMOZIP/AMMORUN/AMMODBG.\r\n"
        "Field-layer .SYS drivers live in C:\\DRIVERS\\*.ASM - built with AMMOSYS, not stubs.\r\n"
        "\r\n"
        "  AMMOASM /?   AMMOSYS /?   AMMOCC /?   AMMOLINK /?   HELP BUILD\r\n"
        "  BUILD HELLO - one-shot from C:\\SAMPLES\\HELLO.ASM\r\n"
        "  AMMOSYS C:\\DRIVERS\\RTXSB.ASM 160  - SB16 audio field driver\r\n"
        "  BUILD DOOMBOOT - field-theory launcher (FIELD.H / FIELD.INC)\r\n"
        "  AMMOLINK UTIL.OBJ MAIN.OBJ\r\n"
        "  AMMORUN MULTI.COM   AMMODBG LOAD HELLO.COM\r\n"
        "  TYPE COMMAND.TXT for full shell reference\r\n",
        encoding="ascii",
    )
    golden = out / "GOLDEN.TXT"
    if golden.is_file():
        text = golden.read_text(encoding="utf-8", errors="replace")
        if "AMMODECOMP" not in text:
            text = text.replace(
                "  DevKit: AMMOASM AMMOCC AMMOLINK BUILD AMMORUN",
                "  DevKit: AMMOASM AMMOCC AMMOLINK AMMODECOMP BUILD AMMORUN AMMODBG",
            )
            text = text.replace(
                "  HELP          Full command reference",
                "  HELP          Full command reference\r\n"
                "  COMMAND /?    Detailed help per command",
            )
            golden.write_text(text, encoding="utf-8")
    print(f"DevKit v4 → {tools} {inc} {samples}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())