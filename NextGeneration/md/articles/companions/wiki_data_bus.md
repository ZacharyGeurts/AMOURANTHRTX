# DATA BUS — 64 Slots, One Telemetry Spine

**Artist rendition:** Alien wires as metaphor for **coupling done wrong**. OCR misread CANVAS SHADERS — this is the **bus map**.

---

`Options::Canvas::DataBus[64]` mirrored to `FieldSocket.data_bus` every dispatch. `FieldLayer::pumpAll` fills slots before GPU work.

| Range | Base | Topic |
|-------|------|-------|
| 0 | REGISTRY_TAG | Pump generation |
| 2–7 | RAM_BASE | Storage / RAID |
| 8–11 | VGA_BASE | Video |
| 12–15 | FAT_BASE | AMMOFAT |
| 32–41 | INPUT_BASE | Keyboard, mouse, pad |
| 42–56 | VIEW_BASE | Viewport, AmmoOS flags |
| 16–23 | Pipeline | FCC float bits |
| 24–28 | Accountant | entropy, boundary, maint, free-E |

**Design:** Bus = slow control plane. Bulk state stays in guest RAM; bus carries **summaries**.

Layers implement `packToDataBus()`. Shader HUD tiles read fixed slots — no FAT scrape per pixel.

**Memorium:** FieldLayer registry maintainers. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).