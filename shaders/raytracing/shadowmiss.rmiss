// assets/shaders/raytracing/shadowmiss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadEXT float visibility;

void main() {
    visibility = 1.0;
}