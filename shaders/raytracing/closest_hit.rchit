#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

#include "../StoneKey.glsl"   // THE SEAL REMAINS UNBROKEN

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool isShadowRay;

layout(binding = 31, set = 0) uniform SceneUBO {
    float time;
    uint  frame;
    vec2  resolution;
    float blastIntensity;   // 0..1 → 0..9000
} ubo;

// ═══════════════════════════════════════════════════════════════
//                  BALLZ OBLITERATION ENGINE v6.9
// ═══════════════════════════════════════════════════════════════
void main()
{
    vec3 N = normalize(attribs);

    // Time-based explosion pulse (you control speed with blastIntensity)
    float t      = ubo.time * 8.0;
    float pulse  = sin(t * 3.14159) * 0.5 + 0.5;
    float boom   = pow(pulse, 0.3) * 12.0;
    float radius = length(gl_WorldRayOriginEXT - vec3(0.0)); // distance from cube center

    // Core blast color — hotter than the surface of Venus
    vec3 magma      = vec3(1.0, 0.35, 0.8);
    vec3 plasma     = vec3(1.0, 0.1, 0.9);
    vec3 nuclear    = vec3(2.5, 0.0, 1.5);

    // Shockwave rings
    float rings = sin(radius * 30.0 - t * 20.0) * 0.5 + 0.5;
    rings = smoothstep(0.45, 0.55, rings);

    // Final annihilation color
    vec3 color = mix(magma, plasma, rings * boom);
    color = mix(color, nuclear, pow(boom, 3.0) * 0.1);

    // Overdrive everything
    color *= 1.0 + boom * ubo.blastIntensity * 20.0;
    color *= 2.0 + rings * 10.0;

    // HDR bloom go brrrrrrr
    color = max(color, vec3(0.0));

    hitValue = color;
}