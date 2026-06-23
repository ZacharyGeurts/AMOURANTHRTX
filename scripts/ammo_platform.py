#!/usr/bin/env python3
"""RTX-DOS Field Die platform constants — single source for install scripts."""

from __future__ import annotations

from rtx_dos_brand import FLOPPY_IMAGE, HD_IMAGE, PRODUCT, VERSION

GUEST_RAM_FAST_MB = 64
REPORTED_RAM_GB = 4
# Logical 4 GiB C: (FAT16 facade + RAID spill). Physical image build stays compact.
HD_SIZE_GB = 4
HD_BUILD_MB = 512
HD_SIZE_MB = HD_BUILD_MB  # mk_ammo_hd physical allocation
FLOPPY_BYTES = 737280
MANIFEST_VERSION = 26
AMOURANTHRTX_VERSION = "2.2.0"

# Legacy aliases (install scripts)
FLOPPY_PATH = FLOPPY_IMAGE
HD_PATH = HD_IMAGE
PRODUCT_NAME = f"{PRODUCT} {VERSION}"