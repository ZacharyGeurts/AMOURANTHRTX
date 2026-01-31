#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main()
{
    // Pure black miss — easy to spot if nothing hit
    hitValue = vec3(0.02, 0.02, 0.04); // very dark blue-gray so it's not pure black
}