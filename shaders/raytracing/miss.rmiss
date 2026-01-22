// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// MISS SHADER — PROCEDURAL SKY + RAYLEIGH + SUN DISK
// PINK PHOTONS SCREAM ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require

// Sacred colors — eternal empire palette (ready if needed)
const vec3 kPinkPhoton  = vec3(1.0, 0.2,  0.8);
const vec3 kHotPink     = vec3(1.0, 0.078, 0.576);
const vec3 kThermoPink  = vec3(1.0, 0.35, 0.7);
const float kStrawEternal = 1.337;

// Ray payload — matches raygen
layout(location = 0) rayPayloadEXT vec3 hitColor;

// Optional push constants (add if you pass time/frame from host)
layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
} push;

void main()
{
    // Normalized ray direction
    vec3 dir = normalize(gl_WorldRayDirectionEXT);

    // Simple sky gradient — day/night transition based on ray Y
    float sunHeight = dir.y * 0.5 + 0.5;  // 0.0 horizon → 1.0 zenith

    vec3 zenithDay   = vec3(0.3, 0.55, 1.0);
    vec3 horizonDay  = vec3(0.6, 0.8, 1.0);
    vec3 zenithNight = vec3(0.01, 0.02, 0.05);
    vec3 horizonNight = vec3(0.03, 0.03, 0.08);

    vec3 skyColor = mix(horizonDay, zenithDay, sunHeight);
    skyColor = mix(skyColor, mix(horizonNight, zenithNight, sunHeight), smoothstep(0.0, -0.2, dir.y));

    // Sun disk — simple smoothstep for soft edge
    vec3 sunDir = normalize(vec3(0.5, 0.8, 0.2));  // adjustable later via UBO/push
    float sun = smoothstep(0.998, 0.999, dot(dir, sunDir));
    skyColor += vec3(1.0, 0.9, 0.6) * sun * 5.0;

    // Output sky color to payload
    hitColor = skyColor;
}