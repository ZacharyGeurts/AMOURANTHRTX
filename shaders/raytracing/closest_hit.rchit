// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// CLOSEST HIT SHADER — PRIMARY INTERSECTION + BASIC SHADING
// PROCEDURAL GRASS + MATERIALS — PINK PHOTONS SCREAM ETERNAL 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

// Sacred colors
const vec3 kPinkPhoton  = vec3(1.0, 0.2,  0.8);
const vec3 kHotPink     = vec3(1.0, 0.078, 0.576);
const vec3 kThermoPink  = vec3(1.0, 0.35, 0.7);
const float kStrawEternal = 1.337;

// Ray payload
layout(location = 0) rayPayloadInEXT vec3 hitColor;

// Hit attributes
hitAttributeEXT vec3 attribs;

// Push constants
layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
} push;

void main() {
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    mat4 objectToWorld = mat4(
        gl_ObjectToWorldEXT[0].x, gl_ObjectToWorldEXT[0].y, gl_ObjectToWorldEXT[0].z, 0.0,
        gl_ObjectToWorldEXT[1].x, gl_ObjectToWorldEXT[1].y, gl_ObjectToWorldEXT[1].z, 0.0,
        gl_ObjectToWorldEXT[2].x, gl_ObjectToWorldEXT[2].y, gl_ObjectToWorldEXT[2].z, 0.0,
        gl_ObjectToWorldEXT[3].x, gl_ObjectToWorldEXT[3].y, gl_ObjectToWorldEXT[3].z, 1.0
    );

    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 normal = normalize((objectToWorld * vec4(worldUp, 0.0)).xyz);

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));

    float diffuse = max(dot(normal, lightDir), 0.0);

    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    float wave = sin(hitPos.x * 10.0 + push.time * 5.0) * 0.05;
    vec3 albedo = vec3(0.1, 0.6, 0.1) + vec3(0.0, wave, 0.0);

    vec3 finalColor = albedo * diffuse + vec3(0.05);

    hitColor = finalColor;
}