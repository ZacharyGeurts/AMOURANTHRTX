// assets/shaders/raytracing/shadow.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT float shadowPayload;

void main()
{
    shadowPayload = 0.0; // light visible
}