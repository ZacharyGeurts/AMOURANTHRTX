// shaders/raytracing/miss.rmiss
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 rayColor;

void main() {
    rayColor = vec3(0.0, 0.0, 1.0); // Blue for miss
}