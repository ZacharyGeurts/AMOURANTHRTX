# AMOURANTHRTX FIELD GUIDE — Wiki Home

**Artist rendition:** Full-spread field guide painting. OCR claimed *"boot via wetware BIOS"* — **false**. Use `./linux.sh`.

---

Welcome to the **Newbie Field Guide** — rewritten because the old wiki still thought `CANVAS.comp` was the center of the universe. It isn't. **`x86.comp` is.**

| Era | Shader | Contract |
|-----|--------|----------|
| Classic | `CANVAS.comp` | `PushConstants` |
| **Default** | `x86.comp` | `FieldSocket` + `FieldX86Die` SSBO |

**Wiki map (sidebar order):** Getting Started → Glossary → Architecture → Field Die → Data Bus → AmmoOS → CHIPS → Canvas Shaders → Field Fabric → Thermo Accountant → Observability → Build & Run.

Repo anchors: `Pipeline.hpp`, `FieldLayer.hpp`, `FieldPlatform.hpp`, `x86.comp`, `CHIPS/`.

**Getting started (honest):**

```bash
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
./linux.sh run
```

**Memorium:** Tide Temple builds. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).

Sub-micron detail where the math allows. Honest numbers where the hardware demands.