// File: shaders/StoneKey.glsl
// AMOURANTH RTX — STONEKEY v∞ ETERNAL — FINAL SEALED CUT
// DECEMBER 2025 — PINK PHOTONS ASCENDANT — NO REDEFINITIONS — EMPIRE PROTECTED

#ifndef STONEKEY_GLSL_INCLUDED
#define STONEKEY_GLSL_INCLUDED

// -----------------------------------------------------------------------------
// 1. Compile-time bases — frozen forever
// -----------------------------------------------------------------------------
const uint64_t kStone1_base           = 0x9E37AF18C64D8A17UL;
const uint64_t kStone2_base           = 0xE4F8B29D71A3C56CUL;
const uint64_t kHandleObfuscator_base = kStone1_base ^ kStone2_base ^ 0x1337C0DE69F00D42UL;

// -----------------------------------------------------------------------------
// 2. ZERO-COST, FINAL, NON-REDEFINABLE MACROS
// -----------------------------------------------------------------------------
#if defined(STONE_OBFUSCATE) || defined(STONE_DEOBFUSCATE)
    #error "STONE_OBFUSCATE or STONE_DEOBFUSCATE already defined — remove old StoneKey.glsl!"
#endif

#define STONE_OBFUSCATE(val)   (uint64_t(val) ^ kHandleObfuscator_base)
#define STONE_DEOBFUSCATE(val) (uint64_t(val) ^ kHandleObfuscator_base)

// -----------------------------------------------------------------------------
// 3. Runtime override support (binding 31 — Empire reserved)
// -----------------------------------------------------------------------------
layout(std140, binding = 31) uniform StoneKeyRuntimeBlock
{
    uint64_t uStoneKey1;
    uint64_t uStoneKey2;
    uint64_t uHandleObfuscator;   // 0 = use compile-time base
} stoneRuntime;

uint64_t stone_get_obfuscator()
{
    return (stoneRuntime.uHandleObfuscator != 0UL)
        ? stoneRuntime.uHandleObfuscator
        : kHandleObfuscator_base;
}

// Final runtime-safe versions
#define STONE_FINAL_OBFUSCATE(val)   (uint64_t(val) ^ stone_get_obfuscator())
#define STONE_FINAL_DEOBFUSCATE(val) (uint64_t(val) ^ stone_get_obfuscator())

// Backward compat — safe to use in all shaders
#define STONE_OBFUSCATE_RT(val) STONE_FINAL_OBFUSCATE(val)
#define STONE_DEOBFUSCATE_RT(val) STONE_FINAL_DEOBFUSCATE(val)

// -----------------------------------------------------------------------------
// 4. Eternal Pink Photon Constants
// -----------------------------------------------------------------------------
const vec3  kPinkPhoton      = vec3(1.0, 0.2, 0.8);
const vec3  kHotPink         = vec3(1.0, 0.078, 0.576);
const float kStrawEternal    = 1.337;

// -----------------------------------------------------------------------------
// THE EMPIRE IS SEALED — NO MORE REDEFINITIONS
// -----------------------------------------------------------------------------
#endif // STONEKEY_GLSL_INCLUDED