#pragma once

// RTX-AMMOS programming toolchain — AMMOASM / AMMOCC / AMMOLINK / AMMORUN.

#include <cstdint>

namespace FieldAmmoToolchain {

constexpr const char* PRODUCT = "RTX-AMMOS DevKit";
constexpr const char* VERSION = "4.0.2026";
constexpr const char* TOOLS_DIR = "C:\\TOOLS\\";
constexpr const char* INC_DIR   = "C:\\AMMOINC\\";

struct Tool {
    const char* name;
    const char* binary;
    const char* desc;
};

inline constexpr Tool kTools[] = {
    {"FIELDC",   "TOOLS\\FIELDC.COM",   "Field Compiler v4 — .fld → .OBJ"},
    {"AMMOASM",  "TOOLS\\AMMOASM.COM",  "MASM subset v4 → AMMO .OBJ"},
    {"AMMOCC",   "TOOLS\\AMMOCC.COM",   "Tiny C compiler v4 → .OBJ"},
    {"AMMOLINK", "TOOLS\\AMMOLINK.COM", "AMMO .OBJ → .COM on C:"},
    {"AMMODECOMP","TOOLS\\AMMODECOMP.COM","COM/OBJ → .LST disassembly"},
    {"AMMOZIP",  "TOOLS\\AMMOZIP.COM",  "PKZIP extract (stored+deflate) → C:"},
    {"AMMORUN",  "TOOLS\\AMMORUN.COM",  "Run .COM via DevKit interpreter"},
    {"AMMODBG",  "TOOLS\\AMMODBG.COM",  "Step/go/BP/unasm debugger v4"},
    {"BUILD",    "(shell)",             "asm|cc + link one-shot"},
    {"AMMOINC",  "AMMOINC\\",           "RTX headers + intrinsics"},
};

inline std::size_t toolCount() noexcept { return sizeof(kTools) / sizeof(kTools[0]); }

} // namespace FieldAmmoToolchain