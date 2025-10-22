#version 460
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D storageImage;

void main() {
    outColor = texture(storageImage, fragTexCoord);
}