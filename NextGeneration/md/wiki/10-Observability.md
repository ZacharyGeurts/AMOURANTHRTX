# Observability — Issues 61–63

> *In memoriam: **Captain Ellie** — ELLIE logs, sealed TotalTime, status blocks that tell the truth at 3 AM.*

<p align="center">
  <img src="../../assets/wiki/physics/observability/01_ellie.png" width="32%" alt="Issue 61 ELLIE grep" />
  <img src="../../assets/wiki/physics/observability/02_amouranth.png" width="32%" alt="Issue 62 Status Block" />
  <img src="../../assets/wiki/physics/observability/03_ci.png" width="32%" alt="Issue 63 HEADLESS CI" />
</p>

| Cover | Issue | Cast | Packs |
|-------|-------|------|-------|
| ELLIE grep | **61** | Captain Ellie · cute | categories · file:line |
| Status block | **62** | Ammo · sexy truth | FPS · VRAM · GPU ms |
| HEADLESS CI | **63** | CI nerd | probes · debug ring |

---

## The Legend

Observability is **romance** in this engine — complete sentences in ELLIE categories, Captain status block every ~5 s, grep patterns you can teach a friend. Issue 61: Ellie with headset, not helpless. Issue 62: Ammo on the status block because magazine cover and log truth coexist. Issue 63: nerd batch at dawn with `HEADLESS=1`.

*If the painting garbles a log line, grep the repo. Markdown wins.*

---

## The Interview

Run from terminal. Watch ELLIE categories. Captain status block: FPS, GPU ms, VRAM, thermo lines.

```bash
grep -E 'STATUS|entropy|Boundary|THERMO' run.log
grep -E 'RTX-DOS|hotswap|aos_load' run.log
```

`RTX_PROBES=1` for compute/RT ms, invocations, AS compaction — zero cost when off.

GPU timestamps feed adaptive scaler + `RTXProbe`. Early status on short headless runs.

`AMOURANTHRTX_NES_FB_SNAP` + frame index for guest VGA PPM dumps.

`AMOURANTHRTX_FIELD_DEBUG=1` → debug ring in x86 high RAM.

Ellie cute on grep. Ammo sexy on status. Nerd sober on CI. Never -1.

---

## Beginner

Run from terminal. Watch ELLIE categories. Captain status block every ~5 s: FPS, GPU ms, VRAM, thermo lines.

---

## Intermediate

Grep patterns above. `RTX_PROBES=1` when you need probe buffer.

---

## Expert

GPU timestamp adaptive path. NES FB snap env. Field debug ring RAM.

---

**Next:** [11-Build-And-Run.md](11-Build-And-Run.md)

**AMOURANTH FOREVER** — branding loud; physics with units.