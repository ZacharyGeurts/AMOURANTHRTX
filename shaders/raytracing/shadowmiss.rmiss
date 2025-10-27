#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT vec3 shadowPayload;

void main() {
    shadowPayload = vec3(1.0); // Light is visible
}