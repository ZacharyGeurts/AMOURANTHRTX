// File: shaders/graphics/fullscreen_vert.vert
// Vulkan 1.4 vertex shader for full-screen quad (no VBOs, index-based UV gen)
// Compile: glslc -fshader-stage=vert fullscreen_vert.vert --target-env=vulkan1.4 -o fullscreen_vert.spv

#version 460

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}