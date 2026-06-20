# FIELD FABRIC — Phi · Thermo · Flow

**Artist rendition:** Propalactic spirals; sober physicists, surprised faces. OCR labeled OBSERVABILITY — **this is fabric**.

---

Three coupled GPU storage images (`RayCanvas::createAnalogFieldFabric`):

| Image | Binding | Metaphor |
|-------|---------|----------|
| **Phi** | 8 | Potential / phase |
| **Thermo** | 9 | Temperature / entropy density |
| **Flow** | 10 | Velocity / momentum |

**FCC** (`Options::AnalogFields`): `TimeScale`, `ThermoAlpha`, `WaveSpeed`, `GateFidelity`, `EntropyFloor`, `InjectStrength`, `PropalacticScale`, `FieldCoupling`, `TeslaBiasStrength`.

Classic: FCC in `PushConstants` + `fieldProbe`. x86: FCC in `data_bus[16..34]`.

**CFL guard:** host scales `waveSpeed`, `thermoAlpha`, `dT` if unstable — prevents NaN blowups on adaptive res changes.

`updateHardwareFromAnalogFields()` feeds `rtx().hardwareFabric` — fabric drives modeled SM thermals.

**Mouse:** probe injection (heat, vortex, phase kick).

**Memorium:** Landauer / Bennett — irreversibility made visible. Poop repo vision. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).