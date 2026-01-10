// Filename: miss.glsl
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// MISS SHADER — BACKGROUND / PROCEDURAL SKY
// PINK PHOTONS SCREAM ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

void main() {
    // Simple procedural sky (day/night cycle based on ray direction)
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    
    // Blue-ish sky gradient
    vec3 skyColor = mix(vec3(0.1, 0.2, 0.8), vec3(0.6, 0.8, 1.0), dir.y * 0.5 + 0.5);
    
    // Optional sun disk (adapt direction/time as needed)
    vec3 sunDir = normalize(vec3(0.5, 0.8, 0.2));
    float sun = smoothstep(0.998, 0.999, dot(dir, sunDir));
    skyColor += vec3(1.0, 0.9, 0.6) * sun * 5.0;
    
    hitColor = skyColor;
}