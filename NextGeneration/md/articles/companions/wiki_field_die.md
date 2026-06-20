# FIELD DIE — SSBO, FieldSocket, Boot

**Artist rendition:** Register-teeth xenomorph. OCR garbled *libx86emu* — name is **FieldX86Core** (libx86emu lineage).

---

GPU-resident x86 model: registers, page tables, **guest RAM** in one SSBO. Host + emulators write; `x86.comp` renders console from same memory.

**Geometry** (`FieldPlatform.hpp`):

- 64 MiB `GUEST_RAM_BYTES`
- VGA text `0xB8000`, gfx `0xA0000`
- C: mirror `0x01000000`

**FieldSocket:** `sealed_time`, `control`, `data_bus[64]`, `address_bus[16]`, mouse, viewport.

**Control flags:** `ControlRtxDos` (64), `ControlHostCpu` (8), `ControlAmmoExec` (1024), `ControlFieldDebugHud` (2048).

**Boot (RTX-DOS):** `FieldDos::bootGuest` → AMMOFAT → chrome seed → `FieldLayer::pumpAll` each frame.

**Hotswap:** `VkPipelineCache` at `assets/cache/vulkan_compute.cache`. Skip: `AMOURANTHRTX_SKIP_HOTSWAP`.

**Memorium:** Big Grin. **Credits:** FieldX86Core, FieldDos, [MEMORIUMS.md](../../meta/MEMORIUMS.md).