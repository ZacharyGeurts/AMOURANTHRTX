// File: shaders/raytracing/miss.miss
// AMOURANTH RTX — Primary Miss Shader — VALHALLA v∞ TURBO
// Empire-sealed, StoneKey-compliant, binding-31 safe

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../StoneKey.glsl"   // Empire-sealed — binding 31 reserved for runtime override

// Primary visibility miss payload (shared with raygen)
layout(location = 0) rayPayloadInEXT vec3 hitValue;

// Optional: StoneKey entropy injection (currently unused, but ready)
// uint64_t key = stone_get_obfuscator();

void main()
{
    // Primary ray missed geometry → signal raygen to use sky gradient
    // Setting payload to zero is the canonical "no hit" indicator
    hitValue = vec3(0.0);
}