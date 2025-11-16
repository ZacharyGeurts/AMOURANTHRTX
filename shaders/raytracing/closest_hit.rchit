// File: shaders/raytracing/closesthit.chit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_gpu_shader_int64 : require   // StoneKey requires uint64_t support

#include "../StoneKey.glsl"   // PINK PHOTONS ETERNAL — APOCALYPSE v3.2 — VALHALLA LOCKED

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    // Barycentric → world normal (your mesh loader already outputs world-space normals in attribs)
    vec3 normal   = normalize(attribs);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));

    float NdotL = max(dot(normal, lightDir), 0.0);

    // AMOURANTH™ SIGNATURE RASPBERRY PINK — forever burned into every fragment
    const vec3 baseColor = vec3(0.90, 0.20, 0.70);
    vec3 diffuse = baseColor * (0.1 + 0.9 * NdotL);

    // Non-zero = hit → raygen will use this instead of sky
    hitValue = diffuse;
}