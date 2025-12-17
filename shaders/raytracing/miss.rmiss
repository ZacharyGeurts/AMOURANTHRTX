#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

layout(set = 0, binding = 7) uniform sampler2D envMap;

void main()
{
    vec3 dir = gl_WorldRayDirectionEXT;
    float theta = atan(dir.z, dir.x);
    float phi = acos(dir.y);
    vec2 envUV = vec2(theta / (2.0 * 3.14159) + 0.5, phi / 3.14159);
    payload = texture(envMap, envUV).rgb;
}