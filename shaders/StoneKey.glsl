// File: shaders/StoneKey.glsl
// AMOURANTH RTX Engine (C) 2025
// EMPIRE-SEALED RUNTIME KEY — BINDING 31 — VALHALLA v∞ TURBO
// PURE GLSL — ZERO C++ SYNTAX — COMPILES FOREVER

#ifndef STONEKEY_GLSL_INCLUDED
#define STONEKEY_GLSL_INCLUDED

// ── COMPILE-TIME BASE KEY — FROZEN IN THE VOID
const uint64_t kStoneObfuscatorBase = 
    0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;

// ── RUNTIME KEY — INJECTED VIA BINDING 31
layout(std140, binding = 31) uniform StoneKeyRuntimeBlock
{
    uint64_t uStoneKey1;
    uint64_t uStoneKey2;
    uint64_t uHandleObfuscator;   // 0 = use compile-time base
} stoneRuntime;

// ── GET CURRENT OBFUSCATOR — PURE GLSL
uint64_t stone_get_obfuscator()
{
    return (stoneRuntime.uHandleObfuscator != 0UL) 
        ? stoneRuntime.uHandleObfuscator 
        : kStoneObfuscatorBase;
}

// ── FINAL MACROS — WORK IN ALL SHADERS
#define STONE_FINAL_OBFUSCATE(val)   (uint64_t(val) ^ stone_get_obfuscator())
#define STONE_FINAL_DEOBFUSCATE(val) (uint64_t(val) ^ stone_get_obfuscator())

// Legacy support
#define STONE_OBFUSCATE_RT(val)  STONE_FINAL_OBFUSCATE(val)
#define STONE_DEOBFUSCATE_RT(val) STONE_FINAL_DEOBFUSCATE(val)

// ── EMPIRE CONSTANTS — PINK PHOTONS ETERNAL
const vec3  kPinkPhoton   = vec3(1.0, 0.2, 0.8);
const vec3  kHotPink      = vec3(1.0, 0.078, 0.576);
const float kStrawEternal = 1.337;

// THE EMPIRE IS SEALED
#endif // STONEKEY_GLSL_INCLUDED