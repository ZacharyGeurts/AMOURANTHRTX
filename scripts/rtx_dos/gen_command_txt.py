#!/usr/bin/env python3
"""Generate C:\\COMMAND.TXT — on-disk help catalog for RTX-DOS 7.0."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from rtx_dos_brand import VERSION_FULL  # noqa: E402

CATALOG = """\
RTX-DOS 7.0 COMMAND REFERENCE
=============================
Zachary Geurts MCSE+I - GPU Field Die shell (Vulkan + AMMOFAT)

Every built-in accepts:  COMMAND /?   HELP topic

FILES
  DIR [/W /V /S]  LS  TREE  CD  PWD  MD  RD  DEL  COPY  REN
  TYPE  MORE  FIND  ATTRIB  WHERE  CLS  EDIT  EDLIN

SYSTEM
  VER  VOL  LABEL  DATE  TIME  PATH  CHKDSK  MEM  MODE
  ECHO  PROMPT  VERBOSE  BOOT  BIOS  GPU  THROTTLE  HELP  ?

FIELD DIE
  DEVICES  PORTS  DRIVES  DRIVERS  FIELD  LAYER  AMMOFAT
  MOUNT  MSCDEX  CDROM  SOUND  SCALE  FONT  THROTTLE

DEVKIT (C:\\TOOLS)
  TOOLS  AMMOASM  AMMOCC  AMMOLINK  AMMODECOMP  AMMORUN  AMMODBG
  FIELDC  BUILD  AMMOCODE  INCLUDE=C:\\AMMOINC  SAMPLES=C:\\SAMPLES

APPS
  QBASIC  PADTEST  BROWSER  AMOURANTHOS  FILECMD  AMMOS

WINDOWS / ERA
  ERA  WIN31  WIN98  LINUX  SETUP31  BOOT31.BAT  TYPE WIN31.TXT

UTILITIES (MIT lineage)
  FDISK  FORMAT  DEBUG  EDLIN  MORE  PRINT  DISKCOPY  CHKDSK

EXAMPLES
  AMMOASM /?          Detailed assembler help
  HELP AMMOCC         Same as AMMOCC /?
  BUILD HELLO         Assemble+link C:\\SAMPLES\\HELLO.ASM
  TYPE FIELDLAY.TXT   Layer stack documentation
  TYPE GOLDEN.TXT     Golden Era manifesto

"""


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "assets" / "dos" / "ammo")
    out.mkdir(parents=True, exist_ok=True)
    body = f"{VERSION_FULL}\r\n{CATALOG.replace(chr(10), chr(13)+chr(10))}"
    (out / "COMMAND.TXT").write_text(body, encoding="ascii", errors="replace")
    print(f"  wrote COMMAND.TXT ({len(body)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())