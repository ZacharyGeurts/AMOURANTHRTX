// assets/shaders/rasterization/fragment.glsl (Fallback raster)
#version 460

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = vec3(0.0, 0.0, -1.0);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 color = texture(texSampler, fragTexCoord).rgb * diff;
    outColor = vec4(color, 1.0);
}