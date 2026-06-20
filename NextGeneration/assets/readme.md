# Folder Structure — Next Generation

*Every folder holds **≤30 files**. All assets and markdown preserved. Paths below are authoritative.*

---

## Tree

```
NextGeneration/

├── assets/
│   ├── heroes/               3 PNG — Issues 28–30 main covers
│   ├── ads/                  7 PNG — Issues 70–75 (+ .gitkeep)
│   └── wiki/
│       ├── stack/            18 PNG — field stack (6 topics × 3)
│       │   ├── architecture/
│       │   ├── field_die/
│       │   ├── data_bus/
│       │   ├── ammoos/
│       │   ├── chips/
│       │   └── canvas_shaders/
│       ├── physics/          9 PNG — fabric · thermo · observability
│       │   ├── field_fabric/
│       │   ├── thermo_accountant/
│       │   └── observability/
│       └── start/            9 PNG — onboarding · glossary · build
│           ├── getting_started/
│           ├── glossary/
│           └── build_and_run/
```

---

## Path cheat sheet

| From | To asset | To memes archive |
|------|----------|------------------|
| `README.md` (root) | `assets/heroes/…` | `../memes/BigGrin/` |
| `md/wiki/*.md` | `../../assets/wiki/{stack\|physics\|start}/<topic>/` | `../../../memes/BigGrin/` |
| `md/meta/*.md` | `../../assets/…` | `../../../memes/BigGrin/` |
| `md/info/*.md` | — | `../../../memes/BigGrin/` |

---

## Wiki topic → asset folder

| Wiki page | Asset path |
|-----------|------------|
| Home | `assets/heroes/` (Issues 28–30) |
| 01-Getting-Started | `assets/wiki/start/getting_started/` |
| 12-Glossary | `assets/wiki/start/glossary/` |
| 11-Build-And-Run | `assets/wiki/start/build_and_run/` |
| 02-Architecture | `assets/wiki/stack/architecture/` |
| 04-Field-Die | `assets/wiki/stack/field_die/` |
| 05-Data-Bus | `assets/wiki/stack/data_bus/` |
| 06-AmmoOS | `assets/wiki/stack/ammoos/` |
| 07-Emulators-CHIPS | `assets/wiki/stack/chips/` |
| 08-Canvas-Shaders | `assets/wiki/stack/canvas_shaders/` |
| 03-Field-Fabric | `assets/wiki/physics/field_fabric/` |
| 09-Thermo-Accountant | `assets/wiki/physics/thermo_accountant/` |
| 10-Observability | `assets/wiki/physics/observability/` |

---

## File counts (max 30 per folder)

| Folder | Files |
|--------|-------|
| `assets/heroes` | 3 |
| `assets/ads` | 7 |
| `assets/wiki/stack/*` | 3 each |
| `assets/wiki/physics/*` | 3 each |
| `assets/wiki/start/*` | 3 each |
| `md/wiki` | 14 |
| `md/articles/companions` | 13 |
| `md/articles/heroes` | 3 |
| `md/meta` | 9 |
| `md/info` | 5 |

---

[Site README](README.md) · [Legends](md/meta/LEGENDS.md) · [Image manifest](md/meta/IMAGE_MANIFEST.md)
