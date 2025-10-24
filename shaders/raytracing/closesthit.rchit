#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

layout(set = 0, binding = 0) uniform accelerationStructureEXT TopLevelAS;

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float alpha;
    vec3 padding;
};

layout(set = 0, binding = 3, std430) readonly buffer MaterialsData {
    Material materials[];
} materialsData;

layout(set = 0, binding = 6) uniform sampler2D envMap;

struct RayPayload {
    vec4 color;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

struct ShadowPayload {
    float visibility;
};

layout(location = 1) rayPayloadEXT ShadowPayload shadowPayload;

hitAttributeEXT vec2 hitTexCoord;

void main() {
    const int primID = gl_PrimitiveID;
    const int matID = gl_InstanceCustomIndexEXT;
    const Material mat = materialsData.materials[matID % 26];

    const vec3 bary = vec3(1.0 - hitTexCoord.x - hitTexCoord.y, hitTexCoord);
    const vec3 worldPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    const vec3 normal = normalize(mat3(gl_ObjectToWorldEXT) * vec3(0.0, 1.0, 0.0));

    const vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    const float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 color = mat.albedo * (NdotL * 0.8 + 0.2);

    shadowPayload.visibility = 1.0;
    traceRayEXT(TopLevelAS, gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF, 1, 1, 3,
                worldPos + normal * 0.001, 0.001, lightDir, 100.0, 1);

    payload.color = vec4(color * shadowPayload.visibility, 1.0);
}