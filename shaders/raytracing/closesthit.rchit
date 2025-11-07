#version 460
#extension GL_EXT_ray_tracing : require

#pragma shader_stage(closesthit)

#include "../../engine/Vulkan/VulkanCommon.hpp"

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 barycentrics;

void main()
{
    const vec3 lightDir = normalize(vec3(-1.0f, -1.0f, -0.5f));
    float NdotL = dot(barycentrics, lightDir);
    NdotL = max(NdotL, 0.1f);
    hitValue = vec3(0.3f, 0.7f, 1.0f) * NdotL;  // sky blue with simple lighting
}