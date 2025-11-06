#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_image_int64 : enable  // If using 64-bit addresses for TLAS, optional

layout(location = 0) rayPayloadInEXT vec4 payload;  // Incoming payload from raygen

// FIXED: Remove 'readonly' — storage images need write access for imageStore
layout(set = 0, binding = 0, rgba8) uniform image2D outputImage;

void main() {
    // DEBUG: Hardcode RED for all misses (sky/background) — expect full red screen if dispatch works
    ivec2 coord = ivec2(gl_LaunchIDEXT.xy);
    imageStore(outputImage, coord, vec4(1.0, 0.0, 0.0, 1.0));  // Red miss color

    // Optional: Use payload if your raygen sets it (e.g., for depth/fade)
    // imageStore(outputImage, coord, payload);  // But start with hardcoded for test
}