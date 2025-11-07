#version 460
#extension GL_EXT_ray_tracing : require

#pragma shader_stage(miss)

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.05f, 0.1f, 0.2f);  // dark space background
}