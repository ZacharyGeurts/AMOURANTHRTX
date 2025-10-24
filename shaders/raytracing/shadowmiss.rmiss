#version 460
#extension GL_EXT_ray_tracing : require

struct ShadowPayload {
    float visibility;
};

layout(location = 1) rayPayloadInEXT ShadowPayload shadowPayload;

void main() {
    shadowPayload.visibility = 1.0;
}