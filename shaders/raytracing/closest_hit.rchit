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

layout(set = 2, binding = 0) uniform sampler2D textures[1024];

void main()
{
    // Use instance custom index as material index (set in LAS)
    uint matIndex = gl_InstanceCustomIndexEXT;

    // Clamp to safe range (materialCount is available in UBO)
    if (matIndex >= ubo.materialCount) {
        matIndex = 0;
    }

    Material mat = matBuffer.materials[matIndex];

    // Simple normal (backface corrected)
    vec3 normal = gl_WorldRayDirectionEXT;
    normal = normalize(-normal);

    // Directional lighting
    float ndotl = max(dot(normal, ubo.sunDirection), 0.0);
    vec3 diffuse = mat.albedo * ndotl * ubo.sunColor * ubo.sunIntensity;

    // Emissive (pink monster glow)
    vec3 emissive = mat.emissiveColor * mat.emissiveStrength * ubo.emissiveIntensity;

    // Base color
    vec3 color = mat.albedo;

    // Texture sampling (if valid)
    if (mat.textureIndex > 0 && mat.textureIndex < 1024) {
        // Simple barycentric UV
        vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
        vec2 uv = bary.yz;
        color *= texture(textures[nonuniformEXT(mat.textureIndex)], uv).rgb;
    }

    // Ambient
    vec3 ambient = mat.albedo * 0.1;

    // Final color
    vec3 finalColor = diffuse + emissive + ambient;

    // Debug pulsing
    if (ubo.debugMode == 1) {
        finalColor *= 0.7 + 0.3 * sin(ubo.time * 3.0);
    }

    hitValue = finalColor;
}