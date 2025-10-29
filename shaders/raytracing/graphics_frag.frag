#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Environment map (same binding as ray tracing miss shader)
layout(set = 0, binding = 2) uniform sampler2D envmap;

void main()
{
    // Convert screen UV → fake ray direction → equirectangular lookup
    vec3 dir = normalize(vec3(inUV - 0.5, 1.0));
    float phi   = atan(dir.z, dir.x);
    float theta = acos(dir.y);
    vec2 envUV  = vec2(phi * 0.5 / 3.14159265359 + 0.5, theta / 3.14159265359);
    outColor = vec4(texture(envmap, envUV).rgb, 1.0);
}