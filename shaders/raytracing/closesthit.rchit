#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) buffer MaterialSSBO {
    vec4 albedo;
    float roughness;
    float metallic;
    float emission;
    uint textureIndex;
} materials[26];
layout(binding = 6, set = 0) uniform sampler2D envMap;
layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT float shadowAttenuation;

void main() {
    uint materialIndex = gl_PrimitiveID % 26;
    vec3 albedo = materials[materialIndex].albedo.rgb;
    float roughness = materials[materialIndex].roughness;
    vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(0.0, 0.0, 1.0, 0.0)).xyz; // Transform normal

    // Simple diffuse lighting
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Shadow ray
    shadowAttenuation = 1.0;
    vec3 shadowOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 1, 0, 1, shadowOrigin, 0.001, lightDir, 10000.0, 1);

    hitValue = albedo * NdotL * shadowAttenuation;
}