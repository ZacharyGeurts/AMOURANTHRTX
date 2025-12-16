#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

layout(binding = 2, set = 0) uniform sampler2D envMap;

layout(push_constant) uniform PushConstants {
    mat4  invView;
    mat4  invProj;
    float totalTime;
    uint  spp;
    uint  frameSeed;
    uint  forceEnvOnly;
    float jitterStrength;
    uint  maxRecursion;
    uint  useEnvSky;
    uint  flipEnvV;
    uint  showHotPink;           // Debug: thermo pink sky when enabled
    float environmentExposure;
    float skyIntensity;
    float environmentRotationY;
} push;

vec2 DirectionToEquirectUV(vec3 dir)
{
    dir = normalize(dir);
    float theta = atan(dir.z, dir.x);
    float phi   = acos(clamp(dir.y, -1.0, 1.0));

    vec2 uv;
    uv.x = theta * (1.0 / (2.0 * 3.14159265359)) + 0.5;
    uv.y = phi   * (1.0 / 3.14159265359);

    if (push.flipEnvV != 0u)
        uv.y = 1.0 - uv.y;

    float rotRad = radians(push.environmentRotationY);
    uv.x = uv.x - 0.5;
    uv.x = uv.x * cos(rotRad) - uv.y * sin(rotRad) + 0.5;
    uv.x = fract(uv.x);

    return uv;
}

void main()
{
    vec3 rayDir = gl_WorldRayDirectionEXT;

    // Debug mode: thermo pink sky instead of envmap
    if (push.showHotPink != 0u)
    {
        payload = vec3(1.0, 0.0, 1.0);  // Pure hot pink for all misses
        return;
    }

    vec2 uv = DirectionToEquirectUV(rayDir);

    vec3 envColor = texture(envMap, uv).rgb;
    envColor *= push.environmentExposure * push.skyIntensity;

    payload = clamp(envColor, vec3(0.0), vec3(65504.0)); // HDR safe
}