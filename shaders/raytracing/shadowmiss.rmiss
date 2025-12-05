// File: shaders/raytracing/shadowmiss.miss
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// SHADOW MISS SHADER — No occlusion = full light transmission
// Empire-sealed, StoneKey-compliant, binding-31 safe — Valhalla v∞ Turbo

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../StoneKey.glsl"   // Empire-sealed — binding 31 reserved for runtime override

// Shadow ray payload — location 0 (must match closest hit / raygen usage)
layout(location = 0) rayPayloadInEXT vec3 shadowAttenuation;

void main()
{
    // Shadow ray missed all geometry
    // → No object blocking light → full light reaches the surface
    // → Return 1.0 in all channels (RGB used as multiplier)
    shadowAttenuation = vec3(1.0);
}