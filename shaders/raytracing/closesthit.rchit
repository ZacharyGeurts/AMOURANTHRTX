#version 460
#extension GL_EXT_ray_tracing : require

#include "VulkanCommon.hpp"

layout(location = 0) rayPayloadEXT vec3 hitValue;
layout(location = 2) rayPayloadInEXT vec3 radiance;

layout(set = 0, binding = 3) buffer MaterialBuffer { MaterialData m[]; } materials;
layout(set = 0, binding = 4) buffer DimensionBuffer { float d[]; } dimensions;

hitAttributeEXT vec3 attribs;

void main()
{
    const uint matIndex = 0; // Simplified
    MaterialData mat = materials.m[matIndex];

    vec3 normal = normalize(gl_WorldRayDirectionEXT * -1.0); // Backface for now
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normal, lightDir), 0.0);

    vec3 diffuse = mat.diffuse.rgb;
    vec3 color = diffuse * (NdotL * 0.8 + 0.2) + mat.emission.rgb;

    radiance = color;
}