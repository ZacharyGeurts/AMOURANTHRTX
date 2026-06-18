// filename: miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

// Minimal valid declaration for MISS stage: only TLAS (binding 7) permitted per host layout stage flags.
// (Full layout 0-10 + reserved dummies + accel decl + #ext in raygen/compute CANVAS; accountant/fields used for thermo in raygen.)
layout(set = 0, binding = 7) uniform accelerationStructureEXT topLevelAS;

void main() {
    // simple gradient sky
    vec3 dir = gl_WorldRayDirectionEXT;
    float t = 0.5 * (dir.y + 1.0);
    hitColor = mix(vec3(0.1, 0.2, 0.8), vec3(0.8, 0.9, 1.0), t);
    // (thermo sky tint opportunity via payload from raygen if desired)
}