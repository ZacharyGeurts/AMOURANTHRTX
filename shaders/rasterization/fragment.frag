#version 460
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D texSampler;
layout(binding = 2, set = 0, rgba32f) uniform image2D denoiseImage;

void main() {
    vec4 color = texture(texSampler, inUV);
    vec4 denoise = imageLoad(denoiseImage, ivec2(gl_FragCoord.xy));
    outColor = mix(color, denoise, 0.5); // Blend texture with denoised image
}