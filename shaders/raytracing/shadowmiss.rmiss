#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 1) rayPayloadInEXT float shadowAttenuation;

void main() {
    shadowAttenuation = 1.0;
}