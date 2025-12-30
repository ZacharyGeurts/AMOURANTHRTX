#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payloadColor;

layout(set = 0, binding = 7) uniform sampler2D envMap;

#include "common.glsl"

void main()
{
    // Simple environment contribution
    vec3 dir = gl_WorldRayDirectionEXT;
    vec2 uv = sphericalUV(dir);
    vec3 env = texture(envMap, uv).rgb;

    // Optional tone mapping / exposure
    env = env / (env + vec3(1.0));
    env = pow(env, vec3(1.0/2.2));

    payloadColor = env;
}