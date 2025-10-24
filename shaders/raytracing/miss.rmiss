#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 6, set = 0) uniform sampler2D envMap;
layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    vec3 direction = gl_WorldRayDirectionEXT;
    vec2 uv = vec2(atan(direction.z, direction.x) / (2.0 * 3.14159), acos(direction.y) / 3.14159);
    hitValue = texture(envMap, uv).rgb;
}