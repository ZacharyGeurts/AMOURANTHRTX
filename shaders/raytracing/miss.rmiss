// Filename: assets/shaders/raytracing/miss.glsl
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

void main() {
    // Simple sky gradient (can be replaced with environment map later)
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    hitColor = mix(vec3(0.6, 0.7, 1.0), vec3(0.1, 0.2, 0.5), t);
}