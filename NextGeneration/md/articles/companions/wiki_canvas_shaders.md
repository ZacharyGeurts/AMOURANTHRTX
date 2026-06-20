# CANVAS SHADERS — x86.comp Meets CANVAS.comp

**Artist rendition:** Shader hotswap orgasmic *metaphorically* — OCR was unhelpful; truth below.

---

Canvases = GLSL `.comp` → `.spv`. Swipe list (`Options::Canvas::SwipeList`):

| Index | Name | Kind |
|-------|------|------|
| 0 | `x86` | Field Die **default** |
| 1+ | `Amouranth`, `energy`, fractals, tributes | Classic |

**Contracts:**

- **x86:** `FieldSocket` + die SSBO; compute-only console
- **Classic:** `PushConstants` + HDR + materials + optional TLAS + audio cmd buffer

**Boot shaders:** `aos_load` instant, `x86` target hotswap (`deferred_hotswap_min_frames = 12`).

**SPIR-V paths:** `assets/shaders/compute/<name>.spv` relative to binary.

**Iteration:** Edit `.comp` → rebuild shaders → swipe or drop `.spv` hot.

Theme canvases (`CANVAS_*`) = x86 field styling variants.

**Memorium:** Inigo Quilez — SDF stepping canon. **Credits:** glslang/SPIR-V toolchain, [MEMORIUMS.md](../../meta/MEMORIUMS.md).