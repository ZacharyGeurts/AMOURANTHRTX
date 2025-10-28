// fragment.glsl - Simple red-orange background renderer
#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Red-orange color: RGB(255, 69, 0) normalized
    outColor = vec4(1.0, 0.27, 0.0, 1.0);
}