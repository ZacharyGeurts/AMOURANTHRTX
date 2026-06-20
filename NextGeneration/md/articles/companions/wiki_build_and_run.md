# BUILD AND RUN — linux.sh, Assets, CI

**Artist rendition:** Water Temple build alien. OCR cmake noise — **correct recipe below**.

---

```bash
./linux.sh           # configure + release
./linux.sh debug     # debug symbols
./linux.sh run       # build + launch
./linux.sh clean
./linux.sh dos       # RTX-DOS HD image
./linux.sh --help
```

Binary: `build/bin/Linux/AMOURANTHRTX`

**Assets** (`FieldDos::assetRoot()`):

```
assets/shaders/compute/*.spv
assets/dos/rtx_dos_hd.img
assets/textures/rtx_font_sdf.png
assets/cache/vulkan_compute.cache
```

**Env vars:** `HEADLESS`, `MAX_FRAMES`, `SKIP_HOTSWAP`, `FIELD_DEBUG`, `NES_FB_SNAP`, `RTX_PROBES`.

**CI template:**

```bash
./linux.sh
AMOURANTHRTX_HEADLESS=1 AMOURANTHRTX_MAX_FRAMES=60 ./build/bin/Linux/AMOURANTHRTX 2>&1 | tee run.log
grep -q STATUS run.log
```

| Failure | Fix |
|---------|-----|
| No `.spv` | Re-run `linux.sh` |
| RTX-DOS warn | `./linux.sh dos` |
| Black HUD | Clean rebuild; check `FIELD_LAYOUT_VERSION` |

**Memorium:** Tide Temple — WATER TEMPLE EDITION banners. **Credits:** CMake, C++23, SDL3, [MEMORIUMS.md](../../meta/MEMORIUMS.md).