// vertex.glsl
#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Fullscreen triangle vertices in clip space
    // Vertex 0: (-1, -1), UV (0, 0)
    // Vertex 1: ( 3, -1), UV (2, 0)
    // Vertex 2: (-1,  3), UV (0, 2)
    // This covers the screen with barycentric interpolation for UV
    
    vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 uvs[3] = vec2[](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));
    
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = uvs[gl_VertexIndex] * 0.5;
}