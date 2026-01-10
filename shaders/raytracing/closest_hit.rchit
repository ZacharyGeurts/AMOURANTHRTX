// Filename: closest_hit.glsl
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// CLOSEST HIT SHADER — PRIMARY INTERSECTION + BASIC SHADING
// PROCEDURAL GRASS / MATERIALS — PINK PHOTONS SCREAM ETERNAL 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;

// Barycentric coordinates from hit
hitAttributeEXT vec3 attribs;

// Push constants (must match your pipeline layout)
layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
} push;

void main() {
    // Compute world-space normal (fallback to up vector transformed to world space)
    // Replace with real vertex normal interpolation when you have vertex data
    mat4 objectToWorld = mat4(
        gl_ObjectToWorldEXT[0].x, gl_ObjectToWorldEXT[0].y, gl_ObjectToWorldEXT[0].z, 0.0,
        gl_ObjectToWorldEXT[1].x, gl_ObjectToWorldEXT[1].y, gl_ObjectToWorldEXT[1].z, 0.0,
        gl_ObjectToWorldEXT[2].x, gl_ObjectToWorldEXT[2].y, gl_ObjectToWorldEXT[2].z, 0.0,
        gl_ObjectToWorldEXT[3].x, gl_ObjectToWorldEXT[3].y, gl_ObjectToWorldEXT[3].z, 1.0
    );

    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 normal = normalize((objectToWorld * vec4(worldUp, 0.0)).xyz);

    // Simple fixed light direction (replace with your sun direction)
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));

    float diffuse = max(dot(normal, lightDir), 0.0);

    // Procedural green grass albedo
    vec3 albedo = vec3(0.1, 0.6, 0.1);

    // Basic shading + ambient
    vec3 finalColor = albedo * diffuse + vec3(0.05);

    // Optional procedural waving grass (using push.time)
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    float wave = sin(hitPos.x * 10.0 + push.time * 5.0) * 0.05;
    finalColor += vec3(0.0, wave, 0.0);

    hitColor = finalColor;
}