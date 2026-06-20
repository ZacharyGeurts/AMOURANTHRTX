# Brain & Bus — How AMOURANTHRTX Thinks on Your GPU

*Long-form info page. Fun. A little silly. Still true.*

---

## The brain (host)

The **host brain** is deliberately tiny — `main.cpp` barely breaks a sweat. It does not simulate the universe. It **orchestrates**:

- SDL3 senses (eyes/ears/hands)
- `Navigator` heartbeat
- `Pipeline::dispatch_canvas` — one breath per frame
- `FieldLayer::pumpAll` — DOS subsystems tick
- ELLIE logs — the engine talking to you in complete sentences

Think of the host as **Rosa at the wheel**. It steers. It does not become the engine.

---

## The bus (data_bus[64])

The **64-slot data bus** is the nervous system between subsystems and the shader HUD:

- Guest RAM holds the *body* (BIOS, DOS, VGA, carts)
- The bus holds *symptoms* telemetry summaries the console can read without autopsying every byte

Slot 42 even tells `x86.comp` whether AmouranthOS desktop or console shell is live. That's not lore — that's `FieldLayer.hpp`.

---

## The die (Field Die = Big Grin)

The **Field Die** is the second brain — an x86-shaped **SSBO** on the GPU:

- Registers, guest RAM, page tables in one blob
- Host and emulators write; `x86.comp` reads and paints the field operations console
- Default runtime. Not a side quest.

When we say **Big Grin**, we mean: *you booted `C:\>` inside Vulkan and lived.*

---

## The fabric (Phi · Thermo · Flow)

Three images = three metaphors for **analog field behavior**:

| Fabric | Vibe |
|--------|------|
| Phi | Potential / phase — the voltage tide |
| Thermo | Heat / entropy — the honest sweat |
| Flow | Velocity — parallel lanes on the die |

The **thermo accountant** is the brain's receipt printer: entropy, boundary activity, maintenance cost, free-energy income. Landauer would nod. Then ask for the log file.

---

## Silly but accurate summary

```
Brain (host) ──orchestrates──▶ Bus (64 slots) ──mirrors──▶ Shader HUD
        │                              │
        └────────── writes ────────────┴──▶ Die (guest RAM)
                                              │
Fabric ◀── coupled every dispatch ────────────┘
```

**Never -1.** The bus keeps moving. Ellie reads the logs. Ammo looks incredible doing it.

[Back to site README](../../README.md)