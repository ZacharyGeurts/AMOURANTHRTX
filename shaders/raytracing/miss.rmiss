// miss.rmiss (or miss.glsl)
#version 460
#extension GL_EXT_ray_tracing : enable

layout(set = 0, binding = 2) uniform sampler2D envMap;

layout(location = 0) rayPayloadInEXT vec3 hitColor;

void main() {
    // Equirectangular projection for envmap sampling
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float phi = atan(dir.z, dir.x) * (1.0 / (2.0 * 3.1415926535)) + 0.5;
    float theta = acos(dir.y) * (1.0 / 3.1415926535);
    vec2 uv = vec2(phi, theta);

    // Sample envmap (HDR assumed)
    hitColor = texture(envMap, uv).rgb;
}