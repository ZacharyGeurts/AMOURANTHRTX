// File: shaders/graphics/fullscreen_vert.vert
#version 460
#extension GL_ARB_gpu_shader_int64 : require

#include "../StoneKey.glsl"

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex & 1) << 1, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}