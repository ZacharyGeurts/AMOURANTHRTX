# THERMO ACCOUNTANT — The Numbers Do Not Lie

**Artist rendition:** Control-room console; OCR swapped AMMOOS headline — **this is telemetry SSBO**.

---

Host-visible struct every dispatch (binding **2**):

```cpp
entropyThisFrame, avgBoundaryThermo, prevMaintCost, freeEnergyIncome, steps
```

| Field | Meaning |
|-------|---------|
| `entropyThisFrame` | Irreversibility this frame |
| `avgBoundaryThermo` | Boundary activity |
| `prevMaintCost` | Price of accumulation / feedback |
| `freeEnergyIncome` | Input + world + time influx |
| `steps` | Dispatch counter |

**Drivers:** F4 accumulation ↑ maint cost. `InjectStrength` ↑ boundary heat. x86 adds `hostHeat` from `FieldX86Emu::hostCyclesLastFrame()`.

Read via: ELLIE `THERMO`, 5 s status block, `data_bus[24..28]` HUD tiles.

**Not claiming joule-accurate die temps** — consistent simulation accounting for tuning and science runs.

```bash
AMOURANTHRTX_HEADLESS=1 AMOURANTHRTX_MAX_FRAMES=300 ./build/bin/Linux/AMOURANTHRTX 2>&1 | grep -i entropy
```

**Memorium:** *The Numbers* — every physicist who asked for logs, not vibes. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).