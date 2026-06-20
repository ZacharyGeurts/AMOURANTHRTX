# THE FIELD DIE — Big Grin x86 on Your GPU (Yes, Really)

**Artist rendition:** Biomechanical die chip with register teeth. Scientists making the face you make when guest RAM **moves**.  
**OCR note:** Painting mixed *"GPU x86 Guest RAM"* with xenomorph VRAM jokes — corrected.

---

Listen. Emulation usually means "pretend on the CPU and pray." **Field Die** means: one SSBO, registers + 8 MiB uint32 RAM view, `FieldSocket` push every dispatch, `x86.comp` reading the **same** bytes the DOS shell writes. Zachary Geurts MCSE+I did not build a screensaver. He built a **field operations console** that happens to boot `C:\>`.

| Quantity | Value |
|----------|-------|
| `GUEST_RAM_BYTES` | 64 MiB fast die RAM |
| `HD_MIRROR_BYTE` | `0x01000000` — hot C: mirror |
| Default cycles/frame | 131072 (`Options::Canvas::CyclesPerFrame`) |

Boot: `./linux.sh dos` prepares `rtx_dos_hd.img`. `FieldDos::bootGuest` seeds mirror. `ControlRtxDos` goes high. F10 requests die reset.

`AMOURANTHRTX_FIELD_DEBUG=1` — HUD for humans who distrust beauty.

**Memorium:** *Big Grin* — libx86emu lineage, FieldX86Core, every `VER` that says Field Die runtime.  
**Credits:** FieldLayer, FieldDos, Pipeline hotswap. [MEMORIUMS.md](../../meta/MEMORIUMS.md).

The alien wants your VRAM. **Give it.** That's the job.