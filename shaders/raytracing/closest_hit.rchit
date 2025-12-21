// assets/shaders/raytracing/closest_hit.rchit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 attribs;

layout(set = 0, binding = 2, std140) uniform DreamUBO {
    float     time;
    uint      frame;
    uint      currentSpp;
    uint      totalSpp;
    float     exposure;
    uint      enableEnvMap;
    uint      hypertraceEnabled;
    uint      denoisingEnabled;
    uint      adaptiveEnabled;
    uint      debugMode;
    float     envIntensity;
    float     envRotation;

    vec2      resolution;
    vec2      jitter;
    vec2      jitterPrev;
    float     nexusScoreThreshold;
    float     hypertraceJitterScale;
    float     _pad0;
    float     _pad1;

    mat4      view;
    mat4      proj;
    mat4      invView;
    mat4      invProj;

    vec4      camPos;
    vec4      camDir;
    float     fov;
    float     aperture;
    float     focusDistance;
    uint      _pad2;

    uint      materialCount;
    uint      activeMaterialIndex;
    float     metallicOverride;
    float     roughnessOverride;
    float     emissiveIntensity;
    uint      enableBlueNoise;
    uint      enableTAA;
    float     taaAlpha;

    vec3      sunDirection;
    float     sunIntensity;
    vec3      sunColor;
    float     fogDensity;
    vec3      fogColor;
    float     _pad3;

    uint      showNexusScore;
    uint      showSppHeatmap;
    uint      showAccumulationCount;
    uint      showGpuTimestamps;
    float     debugFloat1;
    float     debugFloat2;
    float     debugFloat3;
    float     debugFloat4;
} ubo;

// Material struct matching UBO.hpp
struct Material {
    vec3      albedo;
    float     roughness;
    float     metallic;
    float     emissiveStrength;
    float     alpha;
    float     alphaCutoff;
    uint      textureIndex;
    uint      _pad0;
    vec3      emissiveColor;
    float     _pad1;
};

layout(set = 0, binding = 4, std140) readonly buffer Materials {
    Material materials[];
} matBuffer;

// Texture array (set 2)
layout(set = 2, binding = 0) uniform sampler2D textures[1024];

void main()
{
    uint matIndex = gl_InstanceCustomIndexEXT;
    if (matIndex >= ubo.materialCount) {
        matIndex = 0;
    }

    Material mat = matBuffer.materials[matIndex];

    // Simple normal from ray direction (good enough for plane + billboard)
    vec3 normal = -gl_WorldRayDirectionEXT;
    normal = normalize(normal);

    float ndotl = max(dot(normal, ubo.sunDirection), 0.0);
    vec3 diffuse = mat.albedo * ndotl * ubo.sunColor * ubo.sunIntensity;

    vec3 emissive = mat.emissiveColor * mat.emissiveStrength * ubo.emissiveIntensity;

    vec3 color = mat.albedo;

    // Texture sampling
    if (mat.textureIndex > 0 && mat.textureIndex < 1024) {
        // Simple UV from barycentrics
        vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
        vec2 uv = bary.x * vec2(0.0, 0.0) + bary.y * vec2(1.0, 0.0) + bary.z * vec2(0.5, 1.0);
        color *= texture(textures[nonuniformEXT(mat.textureIndex)], uv).rgb;
    }

    vec3 finalColor = diffuse + emissive + color * 0.1; // Small ambient

    if (ubo.debugMode == 1) {
        finalColor *= (0.8 + 0.2 * sin(ubo.time * 2.0));
    }

    hitValue = finalColor;
}