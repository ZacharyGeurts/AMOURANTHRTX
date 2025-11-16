// File: shaders/raytracing/miss.miss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require   // StoneKey needs uint64_t

#include "../StoneKey.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.0); // MISS → sky computation in raygen
}