#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 2) rayPayloadInEXT vec3 radiance;
layout(set = 0, binding = 5) uniform sampler2D envMap;

void main()
{
    vec3 dir = gl_WorldRayDirectionEXT;
    vec2 uv = vec2(atan(dir.z, dir.x) * 0.1591 + 0.5, acos(dir.y) * 0.3183);
    radiance = texture(envMap, uv).rgb;
}