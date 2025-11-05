// assets/shaders/tonemap.vert
#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec2 outUV;

void main()
{
    // Full-screen triangle covering NDC [-1,1] x [-1,1]
    // gl_VertexIndex: 0,1,2
    const vec2 pos[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    const vec2 uv[3] = vec2[](
        vec2(0.0, 1.0),   // bottom-left  → top-left in Vulkan
        vec2(2.0, 1.0),
        vec2(0.0, 2.0)
    );

    vec2 p = pos[gl_VertexIndex];
    outUV  = uv[gl_VertexIndex];

    gl_Position = vec4(p, 0.0, 1.0);
}