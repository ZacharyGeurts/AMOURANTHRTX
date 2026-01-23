#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

// Incoming payload from raygen
layout(location = 0) rayPayloadInEXT vec3 hitValue;

// Shadow payload (separate location)
layout(location = 1) rayPayloadEXT bool shadowHit;

// Barycentric coordinates from hit
hitAttributeEXT vec3 attribs;

// Top-level acceleration structure
layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

// Output storage image (HDR radiance)
layout(set = 0, binding = 1, rgba32f) uniform image2D outputImage;

// Camera UBO as buffer block (no instance name after })
layout(set = 0, binding = 2) uniform CameraBuffer {
    mat4 viewInverse;
    mat4 projInverse;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 prevCameraPos;
    float exposure;
    float totalTime;
    uint randomSeed;
    uint maxDepth;
    uint padding[3];
} camera;

// Push constants
layout(push_constant) uniform PushConstants {
    float time;
    uint frame;
    uint spp;
    uint seedOffset;
} pc;

void main()
{
    // Barycentric coordinates
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Face normal fallback (world space) — simple & no extra extensions
    vec3 worldNormal = normalize(cross(
        gl_WorldRayDirectionEXT,  // arbitrary vector to get plane normal
        gl_WorldRayDirectionEXT + vec3(0.0, 1.0, 0.0)  // offset to avoid zero cross
    ));

    // If you have per-vertex normals in vertex buffer (recommended):
    // vec3 objectNormal = 
    //     vertexNormal0 * barycentrics.x +
    //     vertexNormal1 * barycentrics.y +
    //     vertexNormal2 * barycentrics.z;
    // worldNormal = normalize((gl_ObjectToWorldEXT * vec4(objectNormal, 0.0)).xyz);

    // Simple albedo (white-ish for now — replace with texture later)
    vec3 albedo = vec3(0.9, 0.85, 0.8);

    // Simple directional light (sun-like)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
    float ndotl = max(0.0, dot(worldNormal, lightDir));

    // Direct lighting contribution
    vec3 direct = albedo * ndotl * vec3(1.8, 1.6, 1.4) * 4.0;

    // Shadow ray — cast toward light
    shadowHit = false;
    float shadowDist = 10000.0;
    traceRayEXT(topLevelAS,
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xff,                       // cull mask
                1,                          // sbtRecordOffset for shadow miss
                0,                          // sbtRecordStride
                1,                          // miss index for shadow
                gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * 0.001,
                shadowDist,
                lightDir,
                shadowDist,
                1);  // shadow payload location

    if (shadowHit) {
        direct *= 0.25;  // darkened in shadow
    }

    // Final hit value
    hitValue = direct;
}