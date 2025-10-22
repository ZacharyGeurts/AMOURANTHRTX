// assets/shaders/raytracing/miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT float visibility;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 6) uniform sampler2D envMap;

layout(push_constant) uniform PushConstants {
    vec4 clearColor;
    vec3 cameraPosition;
    vec3 lightDirection;
    float lightIntensity;
    uint samplesPerPixel;
    uint maxDepth;
    uint maxBounces;
    float russianRoulette;
} push;

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float phi = atan(dir.z, dir.x);
    float theta = acos(dir.y);
    vec2 uv = vec2(phi / (2.0 * 3.14159265) + 0.5, theta / 3.14159265);
    hitValue = texture(envMap, uv).rgb * push.lightIntensity;

    if (length(hitValue) < 0.001) {
        hitValue = push.clearColor.rgb;
    }
}