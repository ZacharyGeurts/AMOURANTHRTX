#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadEXT vec3 hitValue;
hitAttributeEXT vec3 attribs;

layout(set = 0, binding = 3) readonly buffer Materials {
    vec4 albedo;
    vec4 emissive;
    vec4 metallicRoughness;  // .x = metallic, .y = roughness
    vec4 padding;
} materials[];

void main() {
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Get material (instance ID or primitive ID based)
    uint matIndex = gl_InstanceID;  // simple — extend with gl_PrimitiveID if needed
    vec3 albedo = materials[matIndex].albedo.rgb;
    vec3 emissive = materials[matIndex].emissive.rgb;
    float metallic = materials[matIndex].metallicRoughness.x;
    float roughness = materials[matIndex].metallicRoughness.y;

    // Simple PBR-ish lighting (placeholder sun from living world)
    vec3 normal = normalize(gl_WorldRayDirectionEXT);  // approximate — use vertex normal in real mesh
    vec3 lightDir = normalize(vec3(1,1,1));  // todo: pull from living world buffer
    float ndotl = max(dot(normal, lightDir), 0.0);

    vec3 diffuse = albedo * (1.0 - metallic) * ndotl;
    vec3 specular = mix(albedo, vec3(0.04), metallic) * pow(max(dot(reflect(-lightDir, normal), -gl_WorldRayDirectionEXT), 0.0), 32.0 / (roughness + 0.001));

    hitValue = diffuse + specular + emissive;
}