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

// Material struct — matches C++ exactly (64 bytes, std140)
struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    float emissiveStrength;
    float alpha;
    float alphaCutoff;
    uint textureIndex;
    uint _pad0;
    vec3 emissiveColor;
    float _pad1;
};

layout(set = 0, binding = 4, std140) readonly buffer Materials {
    Material materials[];
} matBuffer;

// Texture array (set 2, binding 0)
layout(set = 2, binding = 0) uniform sampler2D textures[1024];

void main()
{
    // Instance custom index gives us the material index (set in LAS)
    uint matIndex = gl_InstanceCustomIndexEXT;

    // Clamp to valid range
    matIndex = min(matIndex, ubo.materialCount - 1);

    Material mat = matBuffer.materials[matIndex];

    // Reconstruct world normal from barycentric coordinates
    // For correct lighting on both sides of the ground plane
    const vec3 bary = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(cross(gl_WorldRayDirectionEXT, gl_WorldRayOriginEXT)); // placeholder
    // Proper normal reconstruction requires vertex normals — for now use face normal from ray
    normal = -normalize(gl_WorldRayDirectionEXT); // simple backface lighting

    // Basic directional lighting
    float ndotl = max(dot(normal, ubo.sunDirection), 0.0);
    vec3 diffuse = mat.albedo * ndotl * ubo.sunColor * ubo.sunIntensity;

    // Emissive (pink monster glow)
    vec3 emissive = mat.emissiveColor * mat.emissiveStrength * ubo.emissiveIntensity;

    // Base albedo (ground is gray, monster is pink)
    vec3 color = mat.albedo;

    // Texture support (monster.png for material 1)
    if (mat.textureIndex > 0 && mat.textureIndex < 1024) {
        // Simple UV mapping using barycentrics (good enough for testing)
        vec2 uv = bary.yz; // map barycentric to UV space
        color *= texture(textures[nonuniformEXT(mat.textureIndex)], uv).rgb;
    }

    // Ambient + small indirect term
    vec3 ambient = mat.albedo * 0.1;

    // Final lit color
    vec3 finalColor = diffuse + emissive + ambient + color * 0.05;

    // Debug pulsing for visibility confirmation
    if (ubo.debugMode == 1) {
        finalColor *= (0.7 + 0.3 * sin(ubo.time * 3.0 + float(gl_PrimitiveID)));
    }

    hitValue = finalColor;
}