// filename: closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

hitAttributeEXT vec3 attribs;

// Minimal valid declarations for CLOSEST_HIT stage (per host layout stage flags).
// Allowed here: mats(4), TLAS(7), fields(8/9/10). (Accountant(2), audio(6), HDR(0/1), reserved(3/5) limited to raygen/compute stages only.)
// Full 0-10 match maintained at raygen + compute CANVAS (with accel #ext + dummies). FieldThermo (b9) opportunity noted below.
layout(set = 0, binding = 4) readonly buffer Materials {
    vec4 colors[];      // simplistic: one vec4 color per instance
} materials;

layout(set = 0, binding = 7) uniform accelerationStructureEXT topLevelAS;

layout(set = 0, binding = 8, rgba32f) uniform image2D fieldPhi;
layout(set = 0, binding = 9, rgba32f) uniform image2D fieldThermo;
layout(set = 0, binding = 10, rgba32f) uniform image2D fieldFlow;

void main() {
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // very basic normal from barycentrics (for smooth sphere)
    vec3 normal = normalize(barycentrics);

    // get material color (instance index → material index)
    uint matIndex = gl_InstanceCustomIndexEXT;
    vec3 baseColor = materials.colors[matIndex].rgb;

    // simple lambert + ambient
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
    float diff = max(dot(normal, lightDir), 0.0);

    hitColor = baseColor * (0.2 + 0.8 * diff);

    // Opportunity: fieldThermo (binding 9) could be sampled if pixel coord or bary UVs passed via payload
    // from raygen for local temp-based material modulation (e.g. heat tint, roughness). Accountant at 2
    // is raygen-only per current layout flags — pass values via ray payload for chit use.
}