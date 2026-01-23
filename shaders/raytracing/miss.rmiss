// assets/shaders/raytracing/miss.glsl
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main()
{
    // Simple blue sky with gradient
    vec3 skyColor = mix(vec3(0.2, 0.3, 0.8), vec3(0.0, 0.6, 1.0), gl_WorldRayDirectionEXT.y * 0.5 + 0.5);
    hitValue = skyColor * 0.5; // dimmed for contrast
}