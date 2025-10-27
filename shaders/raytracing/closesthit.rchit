#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT vec3 shadowPayload;
hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS; // Added missing declaration
layout(binding = 2, set = 0) uniform sampler2D gDepth;
layout(binding = 3, set = 0) uniform sampler2D gNormal;
layout(push_constant) uniform PushConstants {
    vec2 resolution;
} pc;

void main() {
    // Barycentric coordinates for triangle
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Simple material: diffuse white
    vec3 albedo = vec3(1.0);
    vec3 normal = texture(gNormal, vec2(gl_LaunchIDEXT.xy) / pc.resolution).xyz;

    // Light direction (hardcoded for simplicity)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Shadow ray
    shadowPayload = vec3(0.0);
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 1, hitPoint, 0.001, lightDir, 1000.0, 1);

    vec3 color = albedo * NdotL * shadowPayload;
    hitValue = color;
}