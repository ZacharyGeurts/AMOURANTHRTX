#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Output to fragment shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;

// Uniforms (Push constants or UBO for MVP matrices)
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
} pc;

void main() {
    // Transform vertex position to clip space
    gl_Position = pc.projection * pc.view * pc.model * vec4(inPosition, 1.0);
    
    // Pass data to fragment shader
    fragNormal = mat3(pc.model) * inNormal; // Transform normal to world space
    fragTexCoord = inTexCoord;
    fragPos = vec3(pc.model * vec4(inPosition, 1.0)); // World-space position
}