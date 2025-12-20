// assets/shaders/raytracing/miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

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

layout(set = 0, binding = 7) uniform sampler2D envMap;

void main() {
    if (ubo.enableEnvMap == 1) {
        vec3 dir = gl_WorldRayDirectionEXT;
        float theta = atan(dir.z, dir.x) + ubo.envRotation * 3.14159 / 180.0;
        float phi = acos(dir.y);
        vec2 uv = vec2(theta / (2.0 * 3.14159), phi / 3.14159);
        hitValue = texture(envMap, uv).rgb * ubo.envIntensity;
    } else {
        // Simple gradient with time modulation to test UBO
        float t = 0.5 + 0.5 * gl_WorldRayDirectionEXT.y;
        hitValue = mix(vec3(0.2, 0.3, 0.8), vec3(0.8, 0.9, 1.0), t) * (0.8 + 0.2 * sin(ubo.time));
    }
    
    // Add sun if enabled
    float sun = pow(max(dot(gl_WorldRayDirectionEXT, ubo.sunDirection), 0.0), 32.0) * ubo.sunIntensity;
    hitValue += sun * ubo.sunColor;
}