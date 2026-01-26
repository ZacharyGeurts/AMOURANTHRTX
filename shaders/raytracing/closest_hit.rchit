#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 attribs;

layout(set = 0, binding = 3) readonly buffer Materials {
    vec4 albedo;
    vec4 emissive;
    vec4 metallicRoughness;     // .x = metallic, .y = roughness
} materials[];

layout(set = 0, binding = 7) readonly buffer LivingWorld {
    vec4 sunDirAndIntensity;    // .xyz = direction, .w = intensity
} world;

void main()
{
    uint matIndex = gl_InstanceID;

    vec3 albedo = materials[matIndex].albedo.rgb;
    vec3 emissive = materials[matIndex].emissive.rgb;
    float metallic = materials[matIndex].metallicRoughness.x;
    float roughness = materials[matIndex].metallicRoughness.y;

    vec3 normal = normalize(gl_WorldRayDirectionEXT);  // placeholder

    vec3 lightDir = normalize(world.sunDirAndIntensity.xyz);
    float sunIntensity = world.sunDirAndIntensity.w;

    float NdotL = max(dot(normal, lightDir), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 kD = albedo * (1.0 - metallic);

    vec3 viewDir = -gl_WorldRayDirectionEXT;
    vec3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0);

    float a = roughness * roughness;
    float D = a * a / (NdotH * NdotH * (a * a - 1.0) + 1.0);
    D /= 3.141592653589793 * D * D;

    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(viewDir, halfVec), 0.0), 5.0);

    vec3 specular = F * D * NdotL * NdotH / max(4.0 * max(NdotL, 0.001), 0.000001);

    vec3 direct = (kD / 3.141592653589793 + specular) * NdotL * sunIntensity;

    hitValue = direct + emissive;
}