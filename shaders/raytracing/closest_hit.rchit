#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

// PUSH CONSTANT — must be declared in EVERY shader that uses it
layout(push_constant) uniform PushConstants {
    float totalTime;
} pc;

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec3 attribs; // barycentrics

struct Material {
    vec4 albedo;
    vec4 emissive;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

void main()
{
    // Get material from instance custom index
    uint matIndex = gl_InstanceCustomIndexEXT;
    vec3 albedo = materialBuffer.materials[matIndex].albedo.rgb;

    // Fake normal from ray direction for quick test lighting
    vec3 normal = normalize(-gl_WorldRayDirectionEXT); // flip for visibility
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float ndotl = max(0.0, dot(normal, lightDir));

    // Simple lit color + pink tint
    vec3 color = albedo * (0.2 + 0.8 * ndotl);
    color += vec3(0.8, 0.2, 0.5) * 0.15; // pink glow

    // Modulate with time for breathing (using push constant)
    float pulse = 0.5 + 0.5 * sin(pc.totalTime * 1.618 + gl_HitTEXT * 10.0);
    color *= pulse;

    hitValue = color;
}