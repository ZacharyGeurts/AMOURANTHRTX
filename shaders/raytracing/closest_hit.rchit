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

// Assuming a simple Material struct; adjust to your actual definition
struct Material {
    vec3 color;
    float roughness;
    float metallic;
    vec3 emissive;
    // etc.
};

layout(set = 0, binding = 4, std140) readonly buffer Materials {
    Material materials[];
} matBuffer;

void main() {
    // Barycentric coords
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    // Assume geometry has normals, but for simple, fake normal
    // In real, interpolate from vertex buffer, but assuming simple triangle
    vec3 normal = normalize(gl_WorldRayDirectionEXT);  // Placeholder; use actual from geometry
    
    // Get material; assume instance or primitive has material index
    uint matIndex = gl_PrimitiveID % ubo.materialCount;  // Placeholder
    Material mat = matBuffer.materials[matIndex];
    
    // Simple lambertian
    float ndotl = max(dot(normal, ubo.sunDirection), 0.0);
    vec3 diffuse = mat.color * ndotl * ubo.sunColor * ubo.sunIntensity;
    
    // Emissive
    vec3 emissive = mat.emissive * ubo.emissiveIntensity;
    
    hitValue = diffuse + emissive;
    
    // Modulate with time to test UBO if needed
    if (ubo.debugMode == 1) {
        hitValue *= (0.8 + 0.2 * sin(ubo.time));
    }
}