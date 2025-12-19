// shaders/raytracing/miss.rmiss
// =============================================================================
// AMOURANTH RTX Engine © 2025 — SACRED PINK MISS — v20.1 — DECEMBER 19, 2025
// BRIGHT PINK VOID — EMPIRE DEMANDS COLOR — NO BLACK EVER
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    // SACRED BRIGHT PINK — FULL INTENSITY
    hitValue = vec3(1.0, 0.2, 0.6);  // Hot pink — highly visible
}