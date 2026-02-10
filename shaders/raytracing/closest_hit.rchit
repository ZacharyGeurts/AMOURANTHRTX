// Filename: assets/shaders/raytracing/closest_hit.glsl
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

hitAttributeEXT vec3 hitPoint;  // from intersection shader

layout(set = 0, binding = 3) readonly buffer Materials {
    vec4 albedo;
    vec4 emission;
    float roughness;
    float metallic;
    // ... add more material properties as needed
} materials[];

layout(set = 0, binding = 4) readonly buffer Primitives {
    vec4 aabbMin;
    vec4 aabbMax;
    mat4 transform;
    uint type;
    uint materialIndex;
    float destruction;
} primitives[];

void main() {
    uint primIndex = gl_PrimitiveID;
    uint matIndex  = primitives[primIndex].materialIndex;

    vec3 albedo   = materials[matIndex].albedo.rgb;
    vec3 emission = materials[matIndex].emission.rgb;

    // Simple shading (normal from AABB face or procedural normal)
    // For now, just return albedo + emission
    hitColor = albedo + emission * 2.0;

    // Optional: add procedural normal / lighting based on primitive type
    // e.g. if (primitives[primIndex].type == 1) { /* sphere normal */ }
}