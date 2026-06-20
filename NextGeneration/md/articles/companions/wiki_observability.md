# OBSERVABILITY — ELLIE, Status Blocks, RTX Probes

**Artist rendition:** Log xenomorph containment breach joke. OCR read DATA BUS — **this is measurement**.

---

**ELLIE:** file:line:function, categories (`PIPELINE`, `CANVAS`, `THERMO`, `RTXPROBE`). Console `level puny|adept|tidewalker`.

**~5 s status (Captain Amouranth / Captain Ellie):** FPS, GPU ms, VRAM, adaptive scale, feature flags, thermo lines. Short headless runs trigger **early** status.

**GPU timestamps** → adaptive scaler + `RTXProbe` when enabled.

**`RTX_PROBES=1`:** compute/RT ms, invocations, AS compaction, SBT align — zero cost when off.

**Field debug:** `AMOURANTHRTX_FIELD_DEBUG=1` → `ControlFieldDebugHud`.

**Grep recipes:**

```bash
grep -E 'entropy|Boundary|prevMaint|freeEnergy|THERMO' run.log
grep -E 'RTX-DOS|AmmoOS|hotswap|aos_load' run.log
```

**Memorium:** Captain Ellie — 3 AM truth. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).