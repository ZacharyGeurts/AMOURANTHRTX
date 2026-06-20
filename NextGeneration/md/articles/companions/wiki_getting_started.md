# GETTING STARTED — Clone, Build, First Run (Dave Perry Special)

**Artist rendition:** CRT bursts `./linux.sh`; staff recoils from biomechanical boot sequence.  
**OCR note:** Painting scrambled step order — corrected below.

---

**Requirements:** Linux (primary), Vulkan 1.4+ GPU, C++23, SDL3 stack via CMake/`linux.sh`.

```bash
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
chmod +x linux.sh
./linux.sh              # configure + build
./linux.sh run          # build if needed + launch
./linux.sh --help       # clean, dos, shader targets
```

**First windowed run:**

1. `aos_load` splash (fast SPIR-V)
2. Background hotswap to `x86` unless `AMOURANTHRTX_SKIP_HOTSWAP=1`
3. AmouranthOS taskbar if desktop enabled
4. ELLIE logs + ~5 s status (FPS, GPU ms, VRAM, thermo)

**Headless smoke:**

```bash
AMOURANTHRTX_HEADLESS=1 AMOURANTHRTX_MAX_FRAMES=60 ./build/bin/Linux/AMOURANTHRTX
```

| Env | Purpose |
|-----|---------|
| `AMOURANTHRTX_HEADLESS=1` | No window |
| `AMOURANTHRTX_MAX_FRAMES=N` | Bounded exit |
| `AMOURANTHRTX_FIELD_DEBUG=1` | Field HUD |
| `RTX_PROBES=1` | RTX probe buffer |

Missing DOS image: `./linux.sh dos`.

**Memorium:** Zachary Geurts, Gladstone MI — human intent meets silicon. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).

Read next: Field Die → Data Bus → Field Fabric.