#ifndef COMMON_GLSL
#define COMMON_GLSL

struct Material {
    vec4 baseColor;
    vec4 emissive;
    vec4 metallicRoughness; // x=metallic, y=roughness, z=unused, w=unused
    uint albedoTexId;
    uint normalTexId;
    uint metallicRoughnessTexId;
    uint emissiveTexId;
    uint alphaTexId;
    float alphaCutoff;
    uint padding[2];
};

vec2 sphericalUV(vec3 dir)
{
    float phi = atan(dir.z, dir.x);
    float theta = acos(dir.y);
    return vec2(phi * 0.15915 + 0.5, theta * 0.31831);
}

// Placeholder - implement proper UV calculation based on your mesh format
vec2 getUV()
{
    // Example using barycentrics and vertex UVs
    return vec2(0.0); // replace with actual UV interpolation
}

#endif