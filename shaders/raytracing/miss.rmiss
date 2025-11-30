#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require

#include "../StoneKey.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(1.000, 0.278, 0.671);   // #FF47AB — AMOURANTH'S TRUE PINK
}