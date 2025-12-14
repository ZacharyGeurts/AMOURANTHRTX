// shaders/raytracing/miss.rmiss
// SIMPLE THERMO PINK MISS SHADER — EMPIRE EDITION
// Binding 31 is sacred to StoneKey — nothing else touches it
// Pure, clean, and eternal. Photons are thermo pink.

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) rayPayloadInEXT vec3 payload;

// HDR cubemap sky (optional fallback)
layout(set = 0, binding = 7) uniform samplerCube envMap;

// Dream control UBO — moved to binding 30 so StoneKey owns 31 forever
layout(set = 0, binding = 30) uniform DreamUBO {
    float time;
    uint  frame;
    vec2  resolution;
    float exposure;
    uint  enableEnvMap;
} ubo;

// StoneKey — binding 31 eternal (the empire demands it)
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
    0x69f8dd5bde1e4239UL;

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

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);

    // Simple HDR sky when enabled
    if (ubo.enableEnvMap != 0)
    {
        vec3 color = texture(envMap, dir).rgb;

        // Basic exposure + gamma
        color = color * ubo.exposure;
        color = pow(color, vec3(1.0 / 2.2));

        payload = color;
        return;
    }

    // Pure thermo pink void — clean and imperial
    const vec3 kThermoPink = vec3(1.0, 0.35, 0.7);

    // Gentle pulsing to prove the UBO is alive
    float pulse = 0.85 + 0.15 * sin(ubo.time * 0.8);

    payload = kThermoPink * pulse;
}