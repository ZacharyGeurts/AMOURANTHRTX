// shaders/StoneKey.glsl — FINAL CLEAN VERSION (compiles everywhere)
#ifndef STONEKEY_GLSL_INCLUDED
#define STONEKEY_GLSL_INCLUDED

// StoneKey runtime block — BINDING 31 ETERNAL
layout(binding = 31) uniform StoneKeyRuntimeBlock
{
    uint64_t uStoneKey1;
    uint64_t uStoneKey2;
    uint64_t uHandleObfuscator;   // 0 = use compile-time base
} stoneRuntime;

// Hard-coded compile-time base (as the empire demands)
const uint64_t kStoneObfuscatorBase = 
    0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;

uint64_t stone_get_obfuscator()
{
    return (stoneRuntime.uHandleObfuscator != 0UL)
        ? stoneRuntime.uHandleObfuscator
        : kStoneObfuscatorBase;
}

#define STONE_FINAL_OBFUSCATE(val)   (uint64_t(val) ^ stone_get_obfuscator())
#define STONE_FINAL_DEOBFUSCATE(val) (uint64_t(val) ^ stone_get_obfuscator())

#define STONE_OBFUSCATE_RT(val)  STONE_FINAL_OBFUSCATE(val)
#define STONE_DEOBFUSCATE_RT(val) STONE_FINAL_DEOBFUSCATE(val)

// Sacred colors
const vec3 kPinkPhoton  = vec3(1.0, 0.2,  0.8);
const vec3 kHotPink     = vec3(1.0, 0.078, 0.576);
const vec3 kThermoPink  = vec3(1.0, 0.35, 0.7);
const float kStrawEternal = 1.337;

#endif // STONEKEY_GLSL_INCLUDED