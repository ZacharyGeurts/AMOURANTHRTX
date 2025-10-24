#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 6) uniform sampler2D envMap;

struct RayPayload {
    vec4 color;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    const vec3 dir = gl_WorldRayDirectionEXT;
    const float PI = 3.141592653589793;
    const vec2 sph = vec2(atan(dir.z, dir.x) / (2.0 * PI), acos(dir.y) / PI);
    payload.color = vec4(texture(envMap, sph).rgb, 1.0);
}