// miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

struct Payload {
    vec3 color;
    vec3 attenuation;
    uint depth;
    bool miss;
};

layout(location = 0) rayPayloadInEXT Payload payload;

layout(binding = 2, set = 0) uniform sampler2D envMap;

layout(push_constant) uniform PushConstants {
    mat4 invView;
    mat4 invProj;
    float totalTime;
    uint spp;
    uint frameSeed;
    uint forceEnvOnly;
    float jitterStrength;
    uint maxRecursion;
    uint useEnvSky;
    uint flipEnvV;
    uint showHotPink;
    float environmentExposure;
    float skyIntensity;
    float environmentRotationY;
    vec3 billboardBaseColor;
    float billboardAlphaCutoff;
    uint billboardUseAlphaBlend;
    uint showNormals;
    uint showUVs;
    uint showWireframe;
    uint enableReflections;
    uint enableShadows;
    uint enableVolumetrics;
    float fogPulseSpeed;
    float fogPulseAmount;
    float lightBobSpeed;
    float lightBobAmplitude;
    float lightOrbitSpeed;
    float lightOrbitAmplitude;
    float lightColorPulseSpeed;
    float lightColorPulseAmount;
} pc;

vec3 sampleEnvironment(vec3 dir)
{
    vec2 uv;
    uv.x = atan(dir.z, dir.x) * (0.5 / 3.14159265359) + 0.5 + (pc.environmentRotationY / (2.0 * 3.14159265359));
    uv.y = acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265359;
    if (pc.flipEnvV == 1) uv.y = 1.0 - uv.y;
    uv.x = fract(uv.x);
    return texture(envMap, uv).rgb * pc.environmentExposure * pc.skyIntensity;
}

void main()
{
    payload.miss = true;

    if (pc.useEnvSky == 1 || pc.forceEnvOnly == 1)
    {
        vec3 dir = gl_WorldRayDirectionEXT;
        payload.color = sampleEnvironment(dir);
    }
    else
    {
        payload.color = vec3(0.0);
    }

    payload.attenuation = vec3(0.0);
}