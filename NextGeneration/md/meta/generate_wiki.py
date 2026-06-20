#!/usr/bin/env python3
"""Generate tiered wiki pages for NextGeneration GitHub site."""
from pathlib import Path

WIKI = Path(__file__).resolve().parent.parent / "wiki"
ASSET = "../../assets/wiki"

# Topic folder → stack | physics | start (see STRUCTURE.md)
FOLDER_GROUP = {
    "getting_started": "start",
    "glossary": "start",
    "build_and_run": "start",
    "architecture": "stack",
    "field_die": "stack",
    "data_bus": "stack",
    "ammoos": "stack",
    "chips": "stack",
    "canvas_shaders": "stack",
    "field_fabric": "physics",
    "thermo_accountant": "physics",
    "observability": "physics",
}

PAGES = [
    {
        "file": "Home.md",
        "title": "AMOURANTHRTX Field Guide",
        "folder": "readme",
        "next": "01-Getting-Started.md",
        "beginner": """Welcome. Default runtime is the **Field Die** (`x86.comp`), not old-school `CANVAS.comp` alone.

**Clone and run:**
```bash
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
./linux.sh run
```

You get `aos_load` splash → AmouranthOS chrome → ELLIE logs with FPS, VRAM, thermo.""",
        "intermediate": """Two canvas kinds share fabric + accountant:

| Kind | Shader | Push |
|------|--------|------|
| `X86Fields` | `x86`, `aos_load` | `FieldSocket` + die SSBO |
| `Classic` | `CANVAS`, `Amouranth`, … | `PushConstants` |

Wiki map follows sidebar: Getting Started → field stack → physics → operations.""",
        "expert": """Repo anchors: `Pipeline.hpp` (dispatch, hotswap), `FieldLayer.hpp` (bus), `FieldPlatform.hpp` (RAM map), `x86.comp`, `CHIPS/`.

`FIELD_LAYOUT_VERSION = 5`. `ProgramsCanvasReady` gates emulators until live `x86` pipeline replaces `aos_load`.

Full memoriums: [MEMORIUMS.md](../meta/MEMORIUMS.md).""",
    },
    {
        "file": "01-Getting-Started.md",
        "title": "Getting Started",
        "folder": "getting_started",
        "next": "12-Glossary.md",
        "beginner": """**Needs:** Linux, Vulkan 1.4+ GPU, C++23, SDL3 (via `linux.sh`).

```bash
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
chmod +x linux.sh
./linux.sh
./linux.sh run
```

**First run:** `aos_load` splash → taskbar/wallpaper → status block ~every 5 s.

**Keys:** F1 adaptive, F7 camera reset, F10 die reset, canvas swipe for shader family.

Missing DOS: `./linux.sh dos`.""",
        "intermediate": """Headless CI pattern:
```bash
AMOURANTHRTX_HEADLESS=1 AMOURANTHRTX_MAX_FRAMES=60 ./build/bin/Linux/AMOURANTHRTX
```

| Env | Effect |
|-----|--------|
| `HEADLESS=1` | No window |
| `MAX_FRAMES=N` | Bounded exit |
| `SKIP_HOTSWAP=1` | Skip background x86 compile |
| `FIELD_DEBUG=1` | Field HUD |
| `RTX_PROBES=1` | RTX probe buffer |

Console: `level puny|adept|tidewalker`.""",
        "expert": """Boot sequence: `boot_x86_canvas()` loads `aos_load`, queues hotswap to `x86`, sets `ProgramsCanvasReady` when Vk pipeline live.

`sync_x86_compile_active` vs `hotswap_compile_active` split boot splash from background compile.

Read next: Field Die → Data Bus → Field Fabric.""",
    },
    {
        "file": "12-Glossary.md",
        "title": "Glossary",
        "folder": "glossary",
        "next": "02-Architecture.md",
        "beginner": """Quick terms:

- **Field Die** — GPU x86 + guest RAM
- **AmmoOS** — desktop shell on the die
- **Data bus** — 64 telemetry slots
- **Fabric** — Phi, Thermo, Flow images
- **x86.comp** — default console shader""",
        "intermediate": """| Term | Meaning |
|------|---------|
| CanvasKind | Classic vs X86Fields |
| FieldSocket | x86 push block |
| FCC | Field Control Constants |
| Hotswap | aos_load → x86 background |
| ThermoAccountant | Entropy SSBO binding 2 |
| ProgramsCanvasReady | Live x86 pipeline flag |""",
        "expert": """Symbols: `rtx()` singleton, `LOAD_SHADER` = `"aos_load"`, `FIELD_LAYOUT_VERSION` = 5, `GUEST_RAM_BYTES` = 64 MiB, `HD_MIRROR_BYTE` = `0x01000000`.

CHIPS: 75 headers under `Navigator/engine/CHIPS/`.""",
    },
    {
        "file": "02-Architecture.md",
        "title": "Architecture",
        "folder": "architecture",
        "next": "04-Field-Die.md",
        "beginner": """Small host, fat GPU:

`main.cpp` → `Navigator` → `RayCanvas` + `Pipeline` + `FieldLayer` + emulators.

Almost everything interesting happens in `Pipeline::dispatch_canvas` and the active `.comp`.""",
        "intermediate": """Per-frame: input → pack FieldSocket or PushConstants → `FieldLayer::pumpAll` → thermo fill → dispatch → present.

`rtx()` holds Vulkan, TLAS (classic), `GPUFabric`, sealed `TotalTime`.

Hybrid HW RT + RTXGI: classic path only (F2/F3). x86 is compute field console.""",
        "expert": """**Classic bindings:** HDR 0–1, accountant 2, materials 4, audio 6, TLAS 7, fabric 8–10.

**Field bindings:** image 0, die SSBO 1, accountant 2, fabric 8–10, chrome 11–14.

`updateHardwareFromAnalogFields()` drives `rtx().hardwareFabric` from fabric aggregates.""",
    },
    {
        "file": "04-Field-Die.md",
        "title": "Field Die",
        "folder": "field_die",
        "next": "05-Data-Bus.md",
        "beginner": """The **Big Grin** die: x86 state + guest RAM on GPU. Host writes RAM; `x86.comp` draws the console.

Default canvas. Not the old planetary raymarch unless you swipe to classic.""",
        "intermediate": """| Quantity | Value |
|----------|-------|
| Guest RAM | 64 MiB fast die |
| VGA text | `0xB8000` |
| C: mirror | `0x01000000` |

`FieldSocket`: time, control, `data_bus[64]`, mouse, viewport.

`ControlRtxDos`, `ControlAmmoExec`, `ControlFieldDebugHud` in control flags.""",
        "expert": """SSBO: registers + `RAM[0x800000/4]` + GDT/IDT/TLB. Header `DIE_HEADER_UINTS = 39`.

Boot: `FieldDos::bootGuest` → AMMOFAT → chrome seed → `pumpAll`.

Cycles: default 131072; host 8086 up to 262144; GPU fallback capped lower.

Pipeline cache: `assets/cache/vulkan_compute.cache`.""",
    },
    {
        "file": "05-Data-Bus.md",
        "title": "Data Bus",
        "folder": "data_bus",
        "next": "06-AmmoOS.md",
        "beginner": """64 slots mirror host telemetry into the shader every frame. HUD reads fixed slots — not full RAM scrapes.""",
        "intermediate": """| Range | Topic |
|-------|-------|
| 0 | Registry tag |
| 2–7 | RAM / RAID |
| 8–11 | VGA |
| 12–15 | FAT |
| 32–41 | Input |
| 42–56 | Viewport / AmmoOS |
| 16–23 | FCC floats |
| 24–28 | Thermo mirror |""",
        "expert": """`FieldLayer` hooks: `syncFromGuest`, `tick`, `packToDataBus`. Pump order: guest RAM authoritative → layers publish summaries.

`address_bus[16]` for I/O windows. Treat bus as control plane, not DMA.""",
    },
    {
        "file": "06-AmmoOS.md",
        "title": "AmmoOS",
        "folder": "ammoos",
        "next": "07-Emulators-CHIPS.md",
        "beginner": """**AmouranthOS** — taskbar, wallpaper, Start menu. Chrome over guest VGA + GPU textures. Not a separate kernel.""",
        "intermediate": """Boot: `FieldAmouranthOs::boot()` → `sync_aos_textures()` → `patchX86ChromeDescriptors()` → `seedChromeRam`.

Bindings 11–14: ammo portrait, wallpaper, icons, RTX font SDF.

Emulators only tick when `emuAdvancesFrames(AppId)` allows.""",
        "expert": """`data_bus[42]`: bit 12 desktop, bit 29 console shell.

Shutdown: `FieldAmouranthShutdown::closeAllGuestApps`. Exit confirm overlay via `FieldAmouranthExitConfirm`.

Font: `scripts/gen_rtx_sdf_font.py` if `rtx_font_sdf.png` missing.""",
    },
    {
        "file": "07-Emulators-CHIPS.md",
        "title": "Emulators & CHIPS",
        "folder": "chips",
        "next": "08-Canvas-Shaders.md",
        "beginner": """Integrated NES, SNES, Genesis, SMS, Atari 2600 — header-only silicon in `CHIPS/`. Carts tick when you launch from Start.""",
        "intermediate": """Pattern in `Pipeline::dispatch_canvas`: if active + `emuAdvancesFrames`, `tick(gr, keys)` + audio `pump()`.

NES **thermo governor** couples burst to field budget.

`FieldAmmoExec` / Doom: separate GPU exec path.""",
        "expert": """Add system: `CHIPS/MySystem/` → `FieldAmmoMySystem.hpp` → `AppId` → dispatch hook → bus packer. Guest RAM = framebuffer truth.

75 CHIPS headers. Audio via SDL3_mixer; DOS via `FieldDevices::pumpAudio`.""",
    },
    {
        "file": "08-Canvas-Shaders.md",
        "title": "Canvas Shaders",
        "folder": "canvas_shaders",
        "next": "03-Field-Fabric.md",
        "beginner": """Swipe canvases in Options. Default index 0 = `x86`. Index 1+ = classic SDF worlds (`Amouranth`, fractals, tributes).""",
        "intermediate": """| Index | Name | Kind |
|-------|------|------|
| 0 | x86 | Field Die |
| 1+ | Amouranth, energy, … | Classic |

`aos_load` boot → background hotswap → `x86`. SPIR-V under `assets/shaders/compute/`.""",
        "expert": """Layouts: `field_descriptor_layout` vs `pipeline_layout`. `active_pipeline_layout()` selects by `currentCanvasKind`.

`CanvasUsesX86Die = true` remaps name `CANVAS` to x86 die shader.

Theme canvases `CANVAS_*` = x86 styling variants (`IsThemeCanvas`).""",
    },
    {
        "file": "03-Field-Fabric.md",
        "title": "Field Fabric",
        "folder": "field_fabric",
        "next": "09-Thermo-Accountant.md",
        "beginner": """Three GPU images evolve each frame: **Phi** (potential), **Thermo** (heat/entropy), **Flow** (velocity). Living background for both canvas paths.""",
        "intermediate": """FCC knobs: `TimeScale`, `ThermoAlpha`, `WaveSpeed`, `GateFidelity`, `EntropyFloor`, `InjectStrength`, `PropalacticScale`, `FieldCoupling`.

Mouse probe injects heat/vortex/phase. CFL guard prevents NaN on res changes.""",
        "expert": """`updateHardwareFromAnalogFields()` → `rtx().hardwareFabric` clocks/util from fabric aggregates.

Recommended ranges: WaveSpeed 0.1–1.5, ThermoAlpha 0.1–2.0, InjectStrength ≤ 5.0.

x86 packs FCC into `data_bus[16..34]`.""",
    },
    {
        "file": "09-Thermo-Accountant.md",
        "title": "Thermo Accountant",
        "folder": "thermo_accountant",
        "next": "10-Observability.md",
        "beginner": """Every dispatch writes entropy, boundary activity, maintenance cost, free-energy income to a small buffer you can grep in logs.""",
        "intermediate": """| Field | Meaning |
|-------|---------|
| entropyThisFrame | Irreversibility this frame |
| prevMaintCost | Price of accumulation (F4) |
| freeEnergyIncome | Input + world influx |
| avgBoundaryThermo | Boundary activity |

F4 on → maint cost rises. That's honest.""",
        "expert": """Landauer metaphor for `prevMaintCost`. x86 adds `hostHeat` from `FieldX86Emu::hostCyclesLastFrame()`.

Mirrored to `data_bus[24..28]`. Phase 1 schematic — not joule-accurate FEM.""",
    },
    {
        "file": "10-Observability.md",
        "title": "Observability",
        "folder": "observability",
        "next": "11-Build-And-Run.md",
        "beginner": """Run from terminal. Watch ELLIE categories. Captain status block every ~5 s: FPS, GPU ms, VRAM, thermo lines.""",
        "intermediate": """```bash
grep -E 'STATUS|entropy|Boundary|THERMO' run.log
grep -E 'RTX-DOS|hotswap|aos_load' run.log
```

`RTX_PROBES=1` for compute/RT ms, invocations, AS compaction — zero cost when off.""",
        "expert": """GPU timestamps feed adaptive scaler + `RTXProbe`. Early status on short headless runs.

`AMOURANTHRTX_NES_FB_SNAP` + frame index for guest VGA PPM dumps.

`AMOURANTHRTX_FIELD_DEBUG=1` → debug ring in x86 high RAM.""",
    },
    {
        "file": "11-Build-And-Run.md",
        "title": "Build & Run",
        "folder": "build_and_run",
        "next": "Home.md",
        "beginner": """```bash
./linux.sh           # build
./linux.sh run       # launch
./linux.sh dos       # RTX-DOS image
./linux.sh --help
```

Binary: `build/bin/Linux/AMOURANTHRTX`. Demos in `demos/` if you skip compile.""",
        "intermediate": """Assets: `assets/shaders/compute/*.spv`, `assets/dos/rtx_dos_hd.img`, `assets/cache/vulkan_compute.cache`.

| Failure | Fix |
|---------|-----|
| No spv | Re-run linux.sh |
| RTX-DOS warn | ./linux.sh dos |
| Black HUD | Clean rebuild, check layout version |""",
        "expert": """CMake: C++23, Vulkan, SDL3_image/mixer, optional LTO release.

CI template in repo; grep `STATUS` gate. Pipeline cache skipped if hotswap still running at exit.

Windows: `windows.ps1` mirrors targets.""",
    },
]


def triptych(folder: str) -> str:
    b = f"{ASSET}/{folder}"
    return f"""<p align="center">
  <img src="{b}/01_{'amouranth' if folder in ('readme','getting_started','glossary','architecture','field_die','data_bus','ammoos','chips','canvas_shaders','field_fabric','build_and_run') else folder.split('_')[0]}.png" width="32%" onerror="this.style.display='none'" />
  <img src="{b}/02_{'ellie' if folder in ('readme','getting_started','glossary','data_bus','ammoos','canvas_shaders','thermo_accountant','observability') else 'professor' if folder in ('glossary','architecture','field_die','field_fabric','thermo_accountant','build_and_run') else 'newbie' if folder=='getting_started' else 'biggrin' if folder=='field_die' else 'techs' if folder=='data_bus' else 'gamers' if folder=='chips' else 'split' if folder=='canvas_shaders' else 'physicists' if folder=='field_fabric' else 'temple'}.png" width="32%" />
  <img src="{b}/03_{'professors' if folder=='readme' else 'amouranth' if folder in ('glossary','architecture','data_bus','chips','canvas_shaders','field_fabric','observability','build_and_run') else 'ellie' if folder in ('getting_started',) else 'professor' if folder in ('field_die','field_fabric','build_and_run') else 'dosfan' if folder=='ammoos' else 'ops' if folder=='thermo_accountant' else 'ci' if folder=='observability' else 'nerd'}.png" width="32%" />
</p>

*Artist renditions · ≤97 words on-image · [ON_IMAGE_TEXT.md](../meta/ON_IMAGE_TEXT.md)*

"""


# Fix triptych - hardcode per folder instead of broken logic
TRIP = {
    "readme": ("01_amouranth.png", "02_ellie.png", "03_professors.png"),
    "getting_started": ("01_amouranth.png", "02_newbie.png", "03_ellie.png"),
    "glossary": ("01_professor.png", "02_ellie.png", "03_amouranth.png"),
    "architecture": ("01_professors.png", "02_amouranth.png", "03_nerd.png"),
    "field_die": ("01_amouranth.png", "02_biggrin.png", "03_professor.png"),
    "data_bus": ("01_ellie.png", "02_techs.png", "03_amouranth.png"),
    "ammoos": ("01_amouranth.png", "02_ellie.png", "03_dosfan.png"),
    "chips": ("01_gamers.png", "02_professor.png", "03_amouranth.png"),
    "canvas_shaders": ("01_amouranth.png", "02_ellie.png", "03_split.png"),
    "field_fabric": ("01_physicists.png", "02_amouranth.png", "03_professor.png"),
    "thermo_accountant": ("01_professor.png", "02_ellie.png", "03_ops.png"),
    "observability": ("01_ellie.png", "02_amouranth.png", "03_ci.png"),
    "build_and_run": ("01_temple.png", "02_amouranth.png", "03_professor.png"),
}


def embed(folder: str) -> str:
    a, b, c = TRIP[folder]
    group = FOLDER_GROUP[folder]
    base = f"{ASSET}/{group}/{folder}"
    return f"""<p align="center">
  <img src="{base}/{a}" width="32%" alt="{folder} cast A" />
  <img src="{base}/{b}" width="32%" alt="{folder} cast B" />
  <img src="{base}/{c}" width="32%" alt="{folder} cast C" />
</p>

*Artist renditions · ≤97 words on-image · [Memoriums](../meta/MEMORIUMS.md)*

"""


def main():
    WIKI.mkdir(parents=True, exist_ok=True)
    for p in PAGES:
        body = f"""# {p['title']}

{embed(p['folder'])}

## Beginner

{p['beginner']}

## Intermediate

{p['intermediate']}

## Expert

{p['expert']}

---

**Next:** [{p['next']}]({p['next']}) · **Memoriums:** [MEMORIUMS.md](../meta/MEMORIUMS.md)

**AMOURANTH FOREVER** — branding loud; physics with units.
"""
        (WIKI / p["file"]).write_text(body)
        print("wrote", p["file"])


if __name__ == "__main__":
    main()