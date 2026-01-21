// shaders/raytracing/closest_hit.glsl
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// CLOSEST HIT SHADER — PRIMARY INTERSECTION + BASIC SHADING
// PROCEDURAL GRASS + MATERIALS — PINK PHOTONS SCREAM ETERNAL 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

// ── StoneKey security — embedded, eternal, no shared file ──────────────────
layout(binding = 31) uniform StoneKeyRuntimeBlock
{
    uint64_t uStoneKey1;
    uint64_t uStoneKey2;
    uint64_t uHandleObfuscator;   // 0 = use compile-time base
} stoneRuntime;

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

layout(location = 0) rayPayloadInEXT vec3 hitColor;

hitAttributeEXT vec3 attribs;

layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
} push;

void main() {
    // Barycentric interpolation (for smooth normals/UVs)
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // World-space normal (placeholder — replace with real vertex normals)
    mat4 objectToWorld = mat4(
        gl_ObjectToWorldEXT[0].x, gl_ObjectToWorldEXT[0].y, gl_ObjectToWorldEXT[0].z, 0.0,
        gl_ObjectToWorldEXT[1].x, gl_ObjectToWorldEXT[1].y, gl_ObjectToWorldEXT[1].z, 0.0,
        gl_ObjectToWorldEXT[2].x, gl_ObjectToWorldEXT[2].y, gl_ObjectToWorldEXT[2].z, 0.0,
        gl_ObjectToWorldEXT[3].x, gl_ObjectToWorldEXT[3].y, gl_ObjectToWorldEXT[3].z, 1.0
    );

    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 normal = normalize((objectToWorld * vec4(worldUp, 0.0)).xyz);

    // Simple fixed light (replace with sun from UBO later)
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));

    float diffuse = max(dot(normal, lightDir), 0.0);

    // Procedural grass albedo (green with wave)
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    float wave = sin(hitPos.x * 10.0 + push.time * 5.0) * 0.05;
    vec3 albedo = vec3(0.1, 0.6, 0.1) + vec3(0.0, wave, 0.0);

    // Basic shading + ambient
    vec3 finalColor = albedo * diffuse + vec3(0.05);

    hitColor = finalColor;
}