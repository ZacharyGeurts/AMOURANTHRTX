# CHIPS — Header-Only Silicon, Real Carts

**Artist rendition:** Ricoh 2A03, Nintendo cart, Genesis chip on biomechanical PCB. OCR dumped thermo telemetry — wrong panel; **this is emulators**.

---

`Navigator/engine/CHIPS/` — **75 headers**. No external emulator DLLs. Systems compose 6502, Z80, 2C02, YM2612, …

```
CHIPS/Common/ → Nes/, Snes/, Genesis/, Sms/, Atari2600/, …
FieldAmmoNes.hpp → tick(), packDataBus(), thermo governor
```

**Dispatch hook** (`Pipeline::dispatch_canvas` x86 branch): if active + `emuAdvancesFrames`, `tick(gr, keys)` + audio `pump()`.

**Thermo governor:** NES burst tied to field thermo budget — emulator ↔ fabric coupling example.

**AmmoExec / Doom:** separate GPU exec path; shares die RAM.

**Add a system:** CHIPS core → `FieldAmmo*` → `AppId` → dispatch hook → bus packer.

**Memorium:** Every mapper header author who never got a credit screen. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).