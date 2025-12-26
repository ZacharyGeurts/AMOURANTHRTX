// assets/shaders/raytracing/miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(set = 0, binding = 7) uniform sampler2D envMap;

layout(set = 0, binding = 2, std140) uniform DreamUBO {
    // ... (same as raygen)
    uint      enableEnvMap;
    float     envIntensity;
    // ...
} ubo;

void main()
{
    if (ubo.enableEnvMap == 0) {
        hitValue = vec3(0.0);
        return;
    }

    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float theta = acos(dir.y);
    float phi = atan(dir.z, dir.x);
    vec2 uv = vec2(phi * 0.1591 + 0.5, theta * 0.3183);

    hitValue = texture(envMap, uv).rgb * ubo.envIntensity;
}