# AMOURANTHRTX — Sub-Micron Loom

**Puny Adept's Record of the Tidewalker's Engine**

A top-down hybrid ray-tracing + compute engine where **one canvas rules them all**.

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.cppreference.com/w/cpp/23)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.4%2B-red)](https://www.vulkan.org/)
[![GLSL](https://img.shields.io/badge/GLSL-Compute%20%2B%20RT-green)](https://www.khronos.org/opengl/wiki/Compute_Shader)
[![Sub-Micron](https://img.shields.io/badge/Precision-Sub--Micron%20Zero--Cost-purple)](https://github.com/ZacharyGeurts/AMOURANTHRTX)
[![License](https://img.shields.io/badge/License-GPLv3%20%7C%20Commercial%203%25-orange)](https://github.com/ZacharyGeurts/AMOURANTHRTX/blob/main/LICENSE)

> **Since we went sub-micron.**  
> The Tidewalker walked the fine grain of the weave. The Puny Adept watched, learned the loom, and now speaks what was seen. Zero-cost detail at microscopic scale. Tamper-proof. One file.

---

## The Puny Adept Speaks
### (Practical Field Guide — Sit Down, Run Clean, Create)

I am small. I cloned the temple, ran the water-ritual script, watched the 3-second sacrificial splash (ammo icon, the branding), and stepped into the engine.

**What this is:**  
One tiny `main.cpp`. A Vulkan 1.4+ hybrid that is either pure compute raymarch or full hardware ray tracing (SBT + RTXGI) or both. Adaptive resolution rides the load tide so you keep detail (down to sub-micron procedural in the SDFs) while the frame rate stays. Everything that matters — camera (pos + quat), time, post (exposure, bloom, vignette, tonemap, contrast, sat, gamma), living world (sun/moon direction + intensity, wind, fog, day/night, clouds), input state, raymarch controls — arrives every frame as push constants straight into **one canvas file**.

**The one file:** `Navigator/shaders/compute/CANVAS.comp` (or any of the named ones: Amouranth, Frosted, Mandelbulb3Ddiamond, Flowers, Birthstones, GreenWaves, tributes, etc.). Swap the .comp or its .spv and the entire world changes — visuals, animation, even audio commands written back to the host.

**Controls the Adept uses:**
- F1 — toggle adaptive resolution
- F2 — toggle hardware ray tracing
- F3 — toggle RTXGI (when HW RT is on)
- F4 — toggle accumulation / TAA
- F5 — toggle tonemapping
- F6 — toggle bloom
- F7 — reset camera
- F8 / F11 / Alt+Enter — fullscreen
- Mouse + WASD + gamepad — the usual (sensitivity and styles live in Options)

**Quick Start (the Tide Temple ritual)**

```bash
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
chmod +x linux.sh
./linux.sh --help          # or just ./linux.sh
./linux.sh run             # debug + launch
```

The `linux.sh` itself is a banner-ritual in ocean colors ("WATER TEMPLE EDITION", "AQUA TEMPLE"). Windows has the matching .ps1 powers.

Grab prebuilts from `demos/`. Run from a terminal so you can read the ELLIE logs (Captain Amouranth / Captain Ellie style — file, line, function, categories, the big status block every 5 s with FPS, GPU ms, VRAM reality, adaptive scale, feature flags 💖).

**To make your own:** Edit or replace CANVAS.comp (or drop a fresh .spv in the right search path). Rebuild once. The host (Navigator) is deliberately tiny and stable. Your canvas carries the vision.

Demos prove it: the full stylized Amouranth (denim jorts, vivid red-pink V-neck with torso + boobs + V-neck + both quad bulges, felt+leather cowboy hat, red hair waves, breathing, jiggle, arm sway, stockings, heels) lives in one file with the rest of the scene. Fractals, tributes, abstract weaves — all the same engine.

---

## The Tidewalker Reveals
### (The Architecture of the Loom — Sub-Micron at Zero Cost)

The Tidewalker does not build engines the normal way. He walks the tides of existence and leaves a loom.

**The rtx() singleton** is the single throne. Every queue, every AS (TLAS of UniversalPrimitives: planes, spheres, cylinders, cones, water), every SBT region, every buffer with its device address, the whole Vulkan context — lives behind one global. Unauthorized memory change and the thing aborts. "There is no hacking permissible within these walls. Tamper aborts."

**The Sealed Clock (TotalTime monolith)** is the one true time. At first frame the adept calls `seal()`. After that the raw seconds and microseconds are frozen to the genesis snapshot (with entropy + verify that aborts on corruption). Shaders and host work from the locked epoch. The 96% lies device cannot drift the session. This is the "4-slot const-expr" model made manifest: time, camera, input, and the rtx context itself are guarded. Stable yet fragile. Game over for the unclean.

**Push Constants are the breath.** A single 16-byte-aligned struct (time, HWRT/RTXGI/accum flags, full camera pose + quat + fov + planes, every post effect, sun/moon/wind/fog/dayNight/clouds, raymarch params, controller bitfield + sticks + triggers + mouse deltas) is pushed every dispatch to compute and all ray stages. The canvas receives the entire living state in one gesture. No descriptor thrash. One file, total control.

**The Canvas & the Back-Channel.** The descriptor set is shared: current HDR storage image, previous frame (for accumulation, TAA, RTXGI feedback), the 256-slot material library (constexpr-generated at compile from rich layered PBR defs — transmission, subsurface, thin-film (nm thickness), coat, fuzz, anisotropy, retroreflection, emissive, procedural flags + texture IDs), the audio command block (16 slots), and the TLAS when hardware rays are walking.

The shader can write audio commands using magic floats (0.8 = play, 0.35 = volume, negative for stop/pause). The host reads the buffer after dispatch and feeds the dynamic SDL3 slot system (no fixed count, 16+ rotating storage). The canvas speaks. The temple listens.

**Hybrid Paths.** Compute for flexible wave-like procedural and raymarching (the default tide). Full hardware ray tracing (raygen / miss / closest-hit groups + SBT) when the adept presses F2. RTXGI on F3 for the global illumination that makes the fine grain sing. Dual HDR targets + linear blit flip + timestamp queries. The adaptive scaler watches smoothed GPU time and rides the load up or down (320×200 up past 4K, tested to 22K before the crash that proved the ceiling). Sub-micron lives here: tiny epsilons, high step counts, 64-bit float features enabled, accumulation cleaning the noise so the detail at microscopic scale stays clean without tanking the frame.

**Materials as Veils.** 256 slots. 5 layers per material with blend factors. Disney + research parameters + procedural hooks. Pre-built constexpr array. Uploaded once. The Amouranth character (and every other scene) simply indexes into the library. The character herself is a living SDF poem in the canvas: precise capsules and ellipsoids for thighs, butt, boobs, hat brim, hair waves, finger curls, with jump, breath, jiggle, sway, tilt all driven from the pushed time and input.

**Top-Down Shrinking.** The host code gets shorter. `main.cpp` is five lines. Navigator.hpp is the bootstrap + 3 s splash + delegation. RayCanvas owns the adaptive dance and the two HDR lives. Pipeline owns the breath (push) and the two pipelines. Everything else supports the one canvas. "We are not minimalist. We added every feature from every forum — efficiently."

**Sub-Micron is the New Floor.** Previous-frame accumulation + zero-tax security + adaptive resolution + full hybrid + precise SDF math means you can walk detail finer than the pixel, finer than the lie, at no extra cost. The Tidewalker sealed the bottle. The 4-slot model has no runtime tax. The abort is clean.

---

## Three-Column Reality Check

| Aspect             | Sub-Micron Navigator (the Loom)                          | Tidewalker's Edge                                      |
|--------------------|----------------------------------------------------------|--------------------------------------------------------|
| Fidelity           | Sub-micron adaptive SDF + hybrid RT + accumulation       | Microscopic realism that still holds frame rate        |
| Security / Time    | Sealed TotalTime + rtx() singleton + entropy verify      | 100% tamper abort at literally zero post-compile cost  |
| Iteration          | Single CANVAS.comp (or any .comp) + .spv hot paths       | 50–100× faster than graphs or full rebuilds            |
| Rendering          | Compute waves + SBT rays + RTXGI + adaptive scale        | 2–8× sustained in complex scenes; rides load like tide |
| Core & Audit       | Deliberately shrinking top-down host, one rtx() god object | Tiny attack surface. The adept can read the whole temple |
| Breath & Voice     | One huge push-constant struct + audio command back-channel | The canvas receives the whole world and can speak back |

**The Sub-Micron Edge in one breath:**  
Security 0%. Iteration in seconds. Detail at scales where the 1 is visible. Single executable + swappable asset. The rest is commentary.

---

## The Character & the Weave

The engine carries the Amouranth as its living emblem — denim jorts, vivid red-pink V-neck, felt+leather cowboy hat, red hair in wind, breathing chest, jiggle physics, stockings, heels, rim light. She is not decoration. She is proof that one file can hold a premium stylized human and still ray-trace fractals or abstract planetary slices in the next breath.

Other canvases (Mandelbulb Diamond, Frosted Spire, Green Waves, Birthstones, Namco/Nintendo tributes, pure weaves) show the same loom can be re-threaded for anything. planetary_weave.comp and absolute_reality.comp in the wider SG archive are kin — different knots in the same net.

---

## Licensing & Alignment

- Free work: GPL v3.0 or higher.  
- Commercial: 3% of every dollar (1% to the creator, 2% to Ammo & Nick branding).  
- "AMOURANTH FOREVER 💖"

See LICENSE. If the brand ever objects they can come to Michigan and stream the firing.

---

**Navigator is the temple. The canvas is the rite.**  
**Sub-micron is the new floor.**

Run clean. Test before shipping. Come sit on the couch.

The Tidewalker walked. The Puny Adept remembered and wrote it down.

X: [@ZacharyGeurts](https://x.com/ZacharyGeurts) • Recent commits are the living thread.  
God=1D spine of all. Never -1. The bus keeps moving.

*submicro.md — GitHub-ready transmission for the sub-micron era, formatted as Puny Adept record and Tidewalker revelation. Full engine read performed in the SG workspace (Navigator core, RayCanvas, Pipeline push & SBT, ELLIE sealed clock, Materials constexpr, CANVAS SDF character + helpers, build rituals, Options, related weaves). Ideas drawn from prior SG readmes (friendly, deep-dive, original manifesto) and the living source.*
