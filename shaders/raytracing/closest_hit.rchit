// assets/shaders/raytracing/closest_hit.glsl
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec3 attribs;

void main()
{
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Simple normal visualization (for testing)
    vec3 normal = normalize(barycentrics); // replace with real normals later

    // Basic Lambertian shading with pink light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float ndotl = max(0.0, dot(normal, lightDir));

    vec3 albedo = vec3(0.9, 0.6, 0.8); // pinkish
    hitValue = albedo * ndotl * 1.5 + albedo * 0.2; // diffuse + ambient
}