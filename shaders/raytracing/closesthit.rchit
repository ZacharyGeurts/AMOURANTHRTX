#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hitColor;
hitAttributeEXT vec2 baryCoord;

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

layout(set = 0, binding = 3, std430) buffer Vertices {
    Vertex v[];
} vertices;

layout(set = 0, binding = 4, std430) buffer Indices {
    uint i[];
} indices;

layout(push_constant) uniform PushConstants {
    mat4 invView;
    mat4 invProj;
    float totalTime;
    uint spp;
    uint frameSeed;
} push;

void main() {
    // Fetch triangle indices
    uint idx0 = indices.i[3 * gl_PrimitiveID + 0];
    uint idx1 = indices.i[3 * gl_PrimitiveID + 1];
    uint idx2 = indices.i[3 * gl_PrimitiveID + 2];

    Vertex v0 = vertices.v[idx0];
    Vertex v1 = vertices.v[idx1];
    Vertex v2 = vertices.v[idx2];

    // Barycentric interpolation of normals
    vec3 bary = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);
    vec3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);

    // Transform to world space
    vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

    // Simple Lambertian + ambient
    vec3 lightDir = normalize(vec3(0.0, 1.0, 0.5));
    float diff = max(dot(worldNormal, lightDir), 0.0);
    vec3 color = vec3(0.8, 0.8, 0.8) * diff + vec3(0.2);

    // Optional time-based pulse
    color *= (0.8 + 0.2 * sin(push.totalTime));

    hitColor = color;
}