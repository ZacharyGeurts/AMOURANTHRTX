// assets/shaders/raytracing/closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT float visibility;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 6) uniform sampler2D envMap;

struct Material {
    vec3 baseColor;
    float metallic;
    float roughness;
    vec3 emission;
    uint _pad;
};

layout(binding = 3, std430) readonly buffer MaterialSSBO {
    Material materials[];
};

layout(binding = 4, std430) readonly buffer DimensionDataSSBO {
    uint frameIndex;
    uvec2 imageDimensions;
    uint _pad[1];
};

layout(push_constant) uniform PushConstants {
    vec4 clearColor;
    vec3 cameraPosition;
    vec3 lightDirection;
    vec3 lightColor;
    float lightIntensity;
    uint samplesPerPixel;
    uint maxDepth;
    uint maxBounces;
    float russianRoulette;
} push;

vec3 sampleEnvMap(vec3 dir) {
    // Simple equirectangular sampling for environment map
    vec2 uv = vec2(atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5, acos(dir.y) / 3.14159265);
    return texture(envMap, uv).rgb;
}

void main() {
    // Material lookup via instance custom index (set per BLAS instance)
    uint matIndex = gl_InstanceCustomIndexEXT;
    Material mat = materials[matIndex];

    // For simple sphere demo; replace with interpolated normal for meshes
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 normal = normalize(hitPoint); // Assume unit sphere at origin (orb)

    vec3 albedo = mat.baseColor;
    vec3 emission = mat.emission;
    vec3 incoming = -normalize(gl_WorldRayDirectionEXT); // Incoming ray direction

    // Point light: Compute direction and distance from hit point to light
    vec3 lightDir = normalize(push.lightDirection - hitPoint);
    float dist = length(push.lightDirection - hitPoint);
    float attenuation = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist); // Quadratic falloff

    // Diffuse lighting with attenuation and light color
    float NdotL = max(dot(normal, lightDir), 0.0);
    visibility = 0.0; // Default: shadowed

    // Shadow ray from hit point toward light position (terminate on first hit, skip hit shader)
    traceRayEXT(topLevelAS, 
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT,
                0xFF, 0 /* sbtOffset */, 1 /* sbtStride */, 1 /* shadow missIndex */,
                hitPoint + normal * 0.001, 0.001, lightDir, dist - 0.001, 1 /* payload */); // tMax = dist to avoid self-shadow

    vec3 direct = albedo * NdotL * push.lightIntensity * visibility * attenuation * push.lightColor;

    // Indirect diffuse: single-bounce in normal direction (limit to primary rays for simplicity)
    vec3 indirectDiffuse = vec3(0.0);
    // Removed depth check to avoid gl_RayDepthEXT; assume single bounce for now
    hitValue = vec3(0.0); // Reset for indirect trace
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT,
                0xFF, 0 /* sbtOffset */, 1 /* sbtStride */, 0 /* missIndex */,
                hitPoint + normal * 0.001, 0.001, normal, 100.0, 0 /* payload */);
    indirectDiffuse = hitValue * (albedo / 3.14159265); // Lambertian approx

    // Specular reflection for metallic surfaces (limit to primary rays)
    vec3 specular = vec3(0.0);
    if (mat.metallic > 0.0) {  // Removed depth check
        // Fresnel approximation for reflection coefficient
        float cosTheta = max(dot(incoming, normal), 0.0);
        float fresnel = pow(1.0 - cosTheta, 5.0) * (1.0 - mat.metallic) + mat.metallic;

        // Reflection direction
        vec3 reflectDir = reflect(-incoming, normal);

        // Trace reflection ray
        hitValue = vec3(0.0); // Reset
        traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT,
                    0xFF, 0 /* sbtOffset */, 1 /* sbtStride */, 0 /* missIndex */,
                    hitPoint + reflectDir * 0.001, 0.001, reflectDir, 100.0, 0 /* payload */);

        // Mix with env map for low roughness (simple IBL approximation)
        vec3 reflectedColor = hitValue;
        if (mat.roughness > 0.0) {
            // For higher roughness, blend towards diffuse or env
            reflectedColor = mix(reflectedColor, sampleEnvMap(reflectDir), mat.roughness);
        }
        specular = reflectedColor * fresnel;
    }

    // Combine: emission + direct (diffuse only for non-metallic) + indirect diffuse + specular
    vec3 diffuseColor = albedo * (1.0 - mat.metallic);
    hitValue = emission + direct * diffuseColor + indirectDiffuse * diffuseColor + specular;

    // Removed Russian Roulette to avoid depth dependency
}