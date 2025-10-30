#version 460
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform sampler2D denoisedImage;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Push constants for exposure control
layout(push_constant) uniform TonemapPush {
    float exposure;
    float gamma;
} pc;

void main()
{
    // Sample HDR denoised image
    vec3 hdrColor = texture(denoisedImage, inUV).rgb;

    // Exposure adjustment
    hdrColor *= pc.exposure;

    // Reinhard tonemapping
    vec3 ldrColor = hdrColor / (hdrColor + vec3(1.0));

    // Gamma correction
    ldrColor = pow(ldrColor, vec3(1.0 / pc.gamma));

    outColor = vec4(ldrColor, 1.0);
}