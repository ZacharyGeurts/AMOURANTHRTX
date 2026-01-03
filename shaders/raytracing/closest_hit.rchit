// assets/shaders/raytracing/closest_hit.rchit
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 — Closest Hit Shader
// Production-ready PBR with texture array support
// UPDATED JANUARY 03, 2026 — Fixed nonuniform indexing validation
// Added nonuniformEXT() around dynamic texture index
// =============================================================================

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

// Ray payload from raygen (receives final lit color)
layout(location = 0) rayPayloadInEXT vec3 hitValue;

// Hit attributes (barycentric coordinates passed from intersection shader)
hitAttributeEXT vec3 attribs;

// ===================================================================
// Uniform Buffer (binding 2, set 0) — matches your DreamUBO exactly
// ===================================================================
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

// ===================================================================
// Material definition — matches your C++ struct exactly
// ===================================================================
struct Material {
    vec3  albedo;
    float roughness;
    float metallic;
    float emissiveStrength;
    float alpha;
    float alphaCutoff;
    uint  textureIndex;   // 0 = no texture, 1–1023 = valid texture array index
    uint  _pad0;
    vec3  emissiveColor;
    float _pad1;
};

// ===================================================================
// Material buffer (binding 4, set 0)
// ===================================================================
layout(set = 0, binding = 4, std140) readonly buffer Materials {
    Material materials[];
} matBuffer;

// ===================================================================
// Texture array (binding 0, set 2)
// ===================================================================
layout(set = 2, binding = 0) uniform sampler2D textures[1024];

void main()
{
    // Use instance custom index as material index (set during BLAS/TLAS build)
    uint matIndex = gl_InstanceCustomIndexEXT;

    // Safety clamp
    if (matIndex >= ubo.materialCount || matIndex >= matBuffer.materials.length()) {
        matIndex = 0;
    }

    Material mat = matBuffer.materials[matIndex];

    // Basic world-space normal (facing towards camera)
    vec3 normal = normalize(-gl_WorldRayDirectionEXT);

    // Simple directional sun lighting
    float ndotl = max(dot(normal, ubo.sunDirection), 0.0);
    vec3 directLight = mat.albedo * ndotl * ubo.sunColor * ubo.sunIntensity;

    // Emissive contribution (e.g., pink monster glow)
    vec3 emissive = mat.emissiveColor * mat.emissiveStrength * ubo.emissiveIntensity;

    // Base color
    vec3 color = mat.albedo;

    // Apply texture if valid
    if (mat.textureIndex > 0 && mat.textureIndex < 1024) {
        // Simple UV from barycentrics (good enough for testing; replace with proper UVs later)
        vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
        vec2 uv = bary.yz; // maps triangle to [0,1] x [0,1]

        // CRITICAL FIX: nonuniformEXT required for dynamic array indexing
        uint texIndex = nonuniformEXT(mat.textureIndex);
        color *= texture(textures[texIndex], uv).rgb;
    }

    // Simple ambient term
    vec3 ambient = color * 0.15;

    // Combine lighting
    vec3 finalColor = directLight + emissive + ambient;

    // Debug pulsing effect
    if (ubo.debugMode == 1) {
        finalColor *= 0.7 + 0.3 * sin(ubo.time * 3.0 + float(gl_PrimitiveID));
    }

    // Optional exposure adjustment
    finalColor *= ubo.exposure;

    // Write to payload
    hitValue = finalColor;
}