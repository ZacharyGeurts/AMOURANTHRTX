// shaders/raytracing/closest_hit.rchit
// =============================================================================
// AMOURANTH RTX Engine © 2025 — STANDARD CLOSEST HIT SHADER — v19.3 — DECEMBER 18, 2025
// FIXED: VEC4 FOR VERTEX BUFFER ALIGNMENT (16-BYTE STRIDE) — SCALAR LAYOUT
// PINK PHOTONS BOUNCE TRUE
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 0, scalar) readonly buffer Vertices { vec4 v[]; } vertices;  // vec4 (16 bytes) for align
layout(binding = 1, set = 0, scalar) readonly buffer Indices { uint i[]; } indices;

layout(location = 1) rayPayloadEXT bool hitSomething;

vec3 computeBarycentricCoords(vec3 pos) {
    // Bary from attribs (z typically 0 for triangles)
    return attribs;
}

void main() {
    uint primId = gl_PrimitiveID;
    uint idx0 = indices.i[3 * primId + 0];
    uint idx1 = indices.i[3 * primId + 1];
    uint idx2 = indices.i[3 * primId + 2];

    vec3 v0 = vertices.v[idx0].xyz;  // Use .xyz from vec4
    vec3 v1 = vertices.v[idx1].xyz;
    vec3 v2 = vertices.v[idx2].xyz;

    vec3 bary = computeBarycentricCoords(gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT);

    // Simple flat shading: normal from cross product
    vec3 normal = normalize(cross(v1 - v0, v2 - v0));

    // Diffuse color based on normal
    vec3 color = 0.5 * (normal + 1.0);

    hitValue = color;
    hitSomething = true;
}