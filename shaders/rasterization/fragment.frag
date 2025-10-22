#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input from vertex shader
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;

// Output color
layout(location = 0) out vec4 outColor;

// Descriptor set for texture and lighting
layout(set = 0, binding = 0) uniform sampler2D texSampler;
layout(set = 0, binding = 1) uniform Lighting {
    vec3 lightPos; // World-space light position
    vec3 lightColor; // Light color/intensity
    vec3 viewPos; // Camera position for specular
} lighting;

void main() {
    // Normalize inputs
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lighting.lightPos - fragPos);
    vec3 viewDir = normalize(lighting.viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // Texture color
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Ambient
    vec3 ambient = 0.1 * lighting.lightColor * texColor.rgb;

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lighting.lightColor * texColor.rgb;

    // Specular (Blinn-Phong)
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * lighting.lightColor;

    // Final color
    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, texColor.a);
}