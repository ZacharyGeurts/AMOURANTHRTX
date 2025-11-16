// File: shaders/raytracing/shadowmiss.miss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require   // StoneKey demands uint64_t

#include "../StoneKey.glsl"   // PINK PHOTONS ETERNAL — APOCALYPSE v3.2

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    // SHADOW MISS → no occlusion → full light transmission
    hitValue = vec3(1.0, 1.0, 1.0);
}