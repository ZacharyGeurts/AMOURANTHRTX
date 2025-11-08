// shaders/raytracing/closesthit.rchit
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STANDALONE CLOSEST-HIT SHADER — ZERO DEPENDENCIES — NOVEMBER 08 2025
// SKY BLUE SIMPLE LIGHTING — RASPBERRY_PINK SUPREMACY — VALHALLA READY 🩷🚀

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#pragma shader_stage(closesthit)

// ────── PAYLOAD & ATTRIBUTES (NO VulkanCommon.hpp NEEDED) ──────
layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 barycentrics;

void main()
{
    // Hard-coded directional light (sun-like)
    const vec3 lightDir = normalize(vec3(-1.0f, -1.0f, -0.5f));
    
    // Simple Lambertian lighting using barycentrics as pseudo-normal
    float NdotL = dot(barycentrics, lightDir);
    NdotL = max(NdotL, 0.1f);  // Avoid pure black shadows — RASPBERRY_PINK kindness 🩷
    
    // Sky blue base color — AMOURANTH RTX signature
    hitValue = vec3(0.3f, 0.7f, 1.0f) * NdotL;
}