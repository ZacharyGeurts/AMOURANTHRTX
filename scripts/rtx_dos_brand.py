#!/usr/bin/env python3
"""RTX-DOS product branding — single source for install, patch, and QA."""

from __future__ import annotations

PRODUCT = "RTX-DOS"
VERSION = "7.0"
AMOURANTHRTX_VERSION = "2.2.0"
AMOURANTHRTX_CODENAME = "Field Everything"
VERSION_FULL = f"{PRODUCT} {VERSION}"
CODENAME = "Field Die"
LINEAGE = "Microsoft MS-DOS MIT (v2/v4) -> RTX-DOS 7.0 Modern DOS — 3.x–6.x program compatible"

# Binary patch strings (must fit original MS-DOS slots)
DOS_VERSION_BIN = b"RTX-DOS version 7.0"
COMMAND_BANNER_BIN = b"RTX-DOS 7.0"

# FAT / install
VOLUME_LABEL = "RTXDOS"
OEM_ID = "RTXDOS70"
HD_IMAGE = "rtx_dos_hd.img"
FLOPPY_IMAGE = "rtx_dos720.img"

# Commercial positioning
TAGLINE = "GPU-bound Super DOSBox for 2026+"
SUBTITLE = "Vulkan Field Die — HD image persistence, infinite virtual hardware"