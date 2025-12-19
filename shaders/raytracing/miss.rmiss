// shaders/raytracing/miss.rmiss
// =============================================================================
// AMOURANTH RTX Engine © 2025 — STANDARD MISS SHADER — v19.3 — DECEMBER 18, 2025
// SKY GRADIENT — VOID PINK — EMPIRE ETERNAL (UNCHANGED)
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool hitSomething;

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);

    // Simple sky gradient
    vec3 sky = mix(vec3(0.5, 0.7, 1.0), vec3(1.0, 1.0, 1.0), dir.y * 0.5 + 0.5);

    hitValue = sky;
    hitSomething = false;
}