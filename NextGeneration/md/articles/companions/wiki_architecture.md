# ARCHITECTURE — Tiny Host, Fat GPU Field Computer

**Artist rendition:** Layer-cake ribs; architects scream at server xenomorph. OCR merged GLOSSARY text — **ignore painting labels**.

---

```
main.cpp → Navigator.hpp → RayCanvas + Pipeline + FieldLayer + CHIPS + Options + rtx()
```

**Two canvas kinds** (`Pipeline::CanvasKind`):

| Kind | Shader | Push |
|------|--------|------|
| `X86Fields` | `x86`, `aos_load` | `FieldSocket` |
| `Classic` | `CANVAS`, `Amouranth`, … | `PushConstants` |

Boot: load `aos_load` → queue hotswap → `ProgramsCanvasReady` when `x86` live.

**`rtx()` singleton:** Vulkan, TLAS (classic RT), `GPUFabric` spiderweb driven by `updateHardwareFromAnalogFields()`, sealed `TotalTime`, tamper abort.

**Field bindings (set 0):** image 0, die SSBO 1, accountant 2, fabric 8–10, chrome textures 11–14. `FIELD_LAYOUT_VERSION = 5`.

Hybrid HW RT + RTXGI: **classic path** (F2/F3). x86 is compute-first field console.

**Memorium:** Tidewalker shrink — host gets shorter, canvas gets louder. **Credits:** Khronos, [MEMORIUMS.md](../../meta/MEMORIUMS.md).