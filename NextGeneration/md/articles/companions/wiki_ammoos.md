# AMMOOS — AmouranthOS on the Field Die

**Artist rendition:** COMMAND.COM went biomechanical. OCR swapped AMMOOS/Field Fabric titles — **this page is the desktop shell**.

---

AmmoOS = **chrome**, not a kernel: taskbar, wallpapers, Start menu, window manager over guest VGA + GPU textures. `FieldAmouranthOs` coordinates.

**Boot on x86 activation:**

1. `FieldAmouranthOs::boot()`
2. `sync_aos_textures()` — wallpaper, Start icon, RTX font SDF
3. `patchX86ChromeDescriptors()` bindings 11–14
4. `seedChromeRam`

**Vulkan textures:**

| Binding | Asset |
|---------|-------|
| 11 | Ammo portrait |
| 12 | Wallpaper |
| 13 | Icon atlas |
| 14 | `rtx_font_sdf.png` |

Emulators tick only when `emuAdvancesFrames(AppId)` allows — no background NES heist.

RTX-DOS: `ControlRtxDos`, `FieldDos::bootGuest`, shell at `C:\>`. Missing HD: `./linux.sh dos`.

**Memorium:** Golden Era Man+Machine. MS-DOS / IBM PC heritage. **Credits:** [MEMORIUMS.md](../../meta/MEMORIUMS.md).