#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

// ---------------------------------------------------------------------
//  Bindings
// ---------------------------------------------------------------------
layout(set = 0, binding = 6) uniform sampler2D envMap;

// ---------------------------------------------------------------------
//  Push Constants
// ---------------------------------------------------------------------
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    uint frameIndex;
    uint maxBounces;
    float time;
    float exposure;
    uint enableDenoise;
    uint enableVolumetrics;
    uint enableEnvMap;
    uint enableGBuffer;
    uint enableAlphaMask;
} pc;

// ---------------------------------------------------------------------
//  Payload – Must match Raygen
// ---------------------------------------------------------------------
struct HitPayload {
    vec3 radiance;
    vec3 attenuation;
    vec3 origin;
    vec3 direction;
    uint depth;
    uint seed;
    bool isShadowRay;
};

layout(location = 0) rayPayloadInEXT HitPayload prd;

// ---------------------------------------------------------------------
//  PCG Random
// ---------------------------------------------------------------------
uint pcg(inout uint state) {
    uint prev = state;
    state = state * 747796405u + 2891336453u;
    uint word = ((prev >> ((prev >> 28u) + 4u)) ^ prev) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand(inout uint seed) {
    return float(pcg(seed)) / 4294967295.0;
}

// ---------------------------------------------------------------------
//  Main – RED-ORANGE SKY
// ---------------------------------------------------------------------
void main() {
    vec3 dir = normalize(prd.direction);
    float t = 0.5 + 0.5 * dir.y;
    vec3 skyColor = mix(vec3(1.0, 0.4, 0.0), vec3(1.0, 0.8, 0.4), t); // Deep → Light orange

    if (pc.enableEnvMap != 0) {
        vec2 uv = vec2(atan(dir.x, dir.z), acos(-dir.y)) * vec2(0.159154943, 0.318309886);
        uv = uv * 0.5 + 0.5;
        uv.y = 1.0 - uv.y;
        vec3 envColor = texture(envMap, uv).rgb;
        skyColor = mix(skyColor, envColor, 0.7);
    }

    // Tone mapping + gamma
    skyColor = skyColor / (skyColor + 1.0);
    skyColor = pow(skyColor, vec3(1.0 / 2.2));

    // Noise
    prd.seed = uint(pc.frameIndex * 73856093u + gl_LaunchIDEXT.x * 19349663u + gl_LaunchIDEXT.y * 83492791u);
    float noise = rand(prd.seed) * 0.015 - 0.0075;
    skyColor += noise;

    prd.radiance = skyColor * prd.attenuation;
    prd.attenuation = vec3(0.0); // Terminate path
}