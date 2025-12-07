// File: shaders/raytracing/shadowmiss.miss
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// SHADOW MISS SHADER — Pure light transmission when no occlusion
// Empire-sealed, StoneKey v∞ compliant, binding-31 immortal — Valhalla v∞ Turbo

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../StoneKey.glsl"   // Empire-sealed — binding 31 reserved for runtime override

// Shadow ray payload — location 0 (must match raygen/closest hit)
layout(location = 0) rayPayloadInEXT vec3 shadowAttenuation;

// Optional: runtime control via push constants (if you ever want soft shadows, etc.)
layout(push_constant) uniform ShadowPush {
    uint  enableSoftShadows;   // 0 = hard shadows (default), 1 = future soft shadow mode
    float shadowBias;
    float _pad[2];
} push;

void main()
{
    // Shadow ray missed all geometry
    // → No object blocking light → full light reaches surface
    // → Return 1.0 in all channels (used as multiplier in lighting)

    shadowAttenuation = vec3(1.0);

    // Future-proof: if we ever add soft shadows, this is where it starts
    // if (push.enableSoftShadows != 0) {
    //     // Reserved for future Valhalla v∞ soft shadow tech
    // }
}