// File: shaders/graphics/fullscreen_vert.vert
#version 460
#extension GL_ARB_gpu_shader_int64 : require   // StoneKey v∞ — mandatory

#include "../StoneKey.glsl"   // PINK PHOTONS ETERNAL — APOCALYPSE v3.2 — VALHALLA LOCKED

layout(location = 0) out vec2 outUV;

void main()
{
    // Classic VBO-less fullscreen triangle → quad trick
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}