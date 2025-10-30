#version 460
#extension GL_GOOGLE_include_directive : enable

// Full-screen triangle using gl_VertexIndex
// No vertex input – we generate positions in clip space directly

layout(location = 0) out vec2 outUV;

void main()
{
    // gl_VertexIndex: 0, 1, 2
    // Map to NDC: (-1,-1), (3,-1), (-1,3) → covers whole screen
    float x = float((gl_VertexIndex & 1) << 2) - 1.0;   //  -1 or 3
    float y = float((gl_VertexIndex & 2) << 1) - 1.0;   //  -1 or 3

    gl_Position = vec4(x, y, 0.0, 1.0);

    // UV: (0,0) at bottom-left, (1,1) at top-right
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 0.5;
}