// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// CLOSEST HIT SHADER — PRIMARY INTERSECTION + BASIC SHADING
// PROCEDURAL GRASS + MATERIALS — PINK PHOTONS SCREAM ETERNAL 💖
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

// Sacred colors — eternal empire palette
const vec3 kPinkPhoton  = vec3(1.0, 0.2,  0.8);
const vec3 kHotPink     = vec3(1.0, 0.078, 0.576);
const vec3 kThermoPink  = vec3(1.0, 0.35, 0.7);
const float kStrawEternal = 1.337;

// Ray payload — matches raygen
layout(location = 0) rayPayloadInEXT vec3 hitColor;

// Hit attributes from intersection
hitAttributeEXT vec3 attribs;

// Push constants — time & frame from host
layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
} push;

void main()
{
    // Barycentric coordinates for smooth interpolation
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Object-to-world matrix (GL_EXT_ray_tracing provides this)
    mat4 objectToWorld = mat4(
        gl_ObjectToWorldEXT[0].x, gl_ObjectToWorldEXT[0].y, gl_ObjectToWorldEXT[0].z, 0.0,
        gl_ObjectToWorldEXT[1].x, gl_ObjectToWorldEXT[1].y, gl_ObjectToWorldEXT[1].z, 0.0,
        gl_ObjectToWorldEXT[2].x, gl_ObjectToWorldEXT[2].y, gl_ObjectToWorldEXT[2].z, 0.0,
        gl_ObjectToWorldEXT[3].x, gl_ObjectToWorldEXT[3].y, gl_ObjectToWorldEXT[3].z, 1.0
    );

    // World-space normal (simple up vector transformed — procedural grass)
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 normal = normalize((objectToWorld * vec4(worldUp, 0.0)).xyz);

    // Simple directional light (can be UBO later)
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));

    // Basic Lambert diffuse
    float diffuse = max(dot(normal, lightDir), 0.0);

    // World hit position for procedural modulation
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // Procedural grass wave — gentle sine over time
    float wave = sin(hitPos.x * 10.0 + push.time * 5.0) * 0.05;

    // Albedo: green base + wave modulation
    vec3 albedo = vec3(0.1, 0.6, 0.1) + vec3(0.0, wave, 0.0);

    // Final shaded color
    vec3 finalColor = albedo * diffuse + vec3(0.05);  // small ambient

    // Output to ray payload
    hitColor = finalColor;
}