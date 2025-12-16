#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

layout(binding = 3, set = 0) uniform sampler2D billboardTex;

hitAttributeEXT vec2 hitAttribs;

layout(push_constant) uniform PushConstants {
    mat4  invView;
    mat4  invProj;
    float totalTime;
    uint  spp;
    uint  frameSeed;
    uint  forceEnvOnly;
    float jitterStrength;
    uint  maxRecursion;
    uint  useEnvSky;
    uint  flipEnvV;
    uint  showHotPink;
    float environmentExposure;
    float skyIntensity;
    float environmentRotationY;
    vec3  billboardBaseColor;   // Tint multiplier (1,1,1) = true texture colors
} push;

void main()
{
    // Barycentric coordinates for the current triangle
    vec3 bary = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);

    // Full 4-vertex UV interpolation for the quad
    // v0: bottom-left  (0,1)
    // v1: top-left     (0,0)
    // v2: top-right    (1,0)
    // v3: bottom-right (1,1)
    vec2 uv = bary.x * vec2(0.0f, 1.0f) +  // v0
              bary.y * vec2(0.0f, 0.0f) +  // v1
              bary.z * vec2(1.0f, 0.0f);   // v2

    // v3 weight = 1 - bary.x - bary.y - bary.z
    float w = 1.0f - bary.x - bary.y - bary.z;
    uv += w * vec2(1.0f, 1.0f);  // v3

    // Vulkan textures are Y-down → flip V to match intended UV orientation
    uv.y = 1.0f - uv.y;

    // Sample the actual WebP texture
    vec3 texColor = texture(billboardTex, uv).rgb;

    // Apply optional tint (set to 1,1,1 in options for true original colors)
    texColor *= push.billboardBaseColor;

    // Debug override: hot pink only when explicitly enabled
    if (push.showHotPink != 0u) {
        payload = vec3(1.0, 0.0, 1.0);  // Hot pink for hit debugging
    } else {
        payload = texColor;  // True texture colors
    }
}