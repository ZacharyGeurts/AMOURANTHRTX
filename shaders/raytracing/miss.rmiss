#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;
layout(set = 0, binding = 7) uniform sampler2D envMap;

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float theta = atan(dir.z, dir.x);
    float phi   = asin(dir.y);

    vec2 uv = vec2(theta, phi) * vec2(0.1591f, 0.3183f);
    uv += 0.5f;
    uv.y = 1.0f - uv.y;

    payload = texture(envMap, uv).rgb;
}