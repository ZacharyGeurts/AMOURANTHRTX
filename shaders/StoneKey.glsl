// File: shaders/StoneKey.glsl
// AMOURANTH RTX Engine © 2025 — StoneKey GLSL Integration v∞
// PINK PHOTONS ETERNAL — APOCALYPSE v3.2
// This file is #included — DO NOT put #version here!

#ifndef STONEKEY_GLSL_INCLUDED
#define STONEKEY_GLSL_INCLUDED

// -----------------------------------------------------------------------------
// 1. Compile-time StoneKey bases (pre-computed Nov 15 2025)
// -----------------------------------------------------------------------------
const uint64_t kStone1_base           = 0x9E37AF18C64D8A17UL;
const uint64_t kStone2_base           = 0xE4F8B29D71A3C56CUL;
const uint64_t kHandleObfuscator_base = kStone1_base ^ kStone2_base ^ 0x1337C0DE69F00D42UL;

// -----------------------------------------------------------------------------
// 2. Zero-cost XOR obfuscation
// -----------------------------------------------------------------------------
uint64_t stone_obfuscate(uint64_t v, uint64_t key)   { return v ^ key; }
uint64_t stone_deobfuscate(uint64_t v, uint64_t key) { return v ^ key; }

#define STONE_OBFUSCATE(val)   stone_obfuscate(uint64_t(val), kHandleObfuscator_base)
#define STONE_DEOBFUSCATE(val) stone_deobfuscate(uint64_t(val), kHandleObfuscator_base)

// -----------------------------------------------------------------------------
// 3. Fingerprint (compile-time constant)
// -----------------------------------------------------------------------------
const uint64_t kStoneFingerprint = ((kStone1_base ^ kStone2_base) * 0x517cc1b727220a95UL);

// -----------------------------------------------------------------------------
// 4. Optional runtime override (push via uniform if you ever need it)
// -----------------------------------------------------------------------------
layout(std140, binding = 31) uniform StoneKeyRuntimeBlock
{
    uint64_t uStoneKey1;
    uint64_t uStoneKey2;
    uint64_t uHandleObfuscator;
};

uint64_t get_obfuscator_key()
{
    return (uHandleObfuscator != 0UL) ? uHandleObfuscator : kHandleObfuscator_base;
}

#endif // STONEKEY_GLSL_INCLUDED