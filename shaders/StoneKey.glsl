// assets/shaders/StoneKey.glsl
// STONEKEY v∞ — BINDING 31 — NOVEMBER 30 2025 — FINAL CANON
// DO NOT INCLUDE #version OR EXTENSIONS — ONLY IN SHADERS

#ifndef STONEKEY_GLSL
#define STONEKEY_GLSL

// ── Compile-time bases (never logged, never visible)
const uint64_t kStone1_base           = 0x9E37AF18C64D8A17UL;
const uint64_t kStone2_base           = 0xE4F8B29D71A3C56CUL;
const uint64_t kObfuscator_base       = kStone1_base ^ kStone2_base ^ 0x1337C0DE69F00D42UL;

// ── Zero-cost XOR
uint64_t stone_xor(uint64_t v, uint64_t k) { return v ^ k; }

// ── BINDING 31 — THE EMPIRE'S SEAL — ONE DECLARATION, ALL SHADERS SEE IT
layout(std140, binding = 31) uniform StoneKeyBlock
{
    uint64_t uKey1;
    uint64_t uKey2;
    uint64_t uObfuscator;
    uint64_t uPinkVoid;        // 1 = PURE PINK, 0 = raygen sky
};

#endif