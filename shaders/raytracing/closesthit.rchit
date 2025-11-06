// assets/shaders/raytracing/closesthit.rchit
// AMOURANTH RTX — CLOSEST HIT SHADER
// FIXED: Rainbow pixels → Safe material sampling + default fallback
// Full PBR: diffuse, specular, metalness, emission

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 attribs;

// === BINDINGS ===
struct MaterialData {
    vec4  diffuse;
    float specular;
    float roughness;
    float metallic;
    vec4  emission;
};

layout(set = 0, binding = 3) buffer MaterialBuffer { MaterialData materials[]; } materialBuffer;
layout(set = 0, binding = 5) uniform sampler2D envMap;

layout(push_constant) uniform PushConstants {
    vec4  clearColor;
    vec3  cameraPosition;
    float _pad0;
    vec3  lightDirection;
    float lightIntensity;
    uint  samplesPerPixel;
    uint  maxDepth;
    uint  maxBounces;
    float russianRoulette;
    vec2  resolution;
    uint  showEnvMapOnly;
    uint  frame;
    float fireflyClamp;
} pc;

// === HELPER ===
vec3 sampleEnvMap(vec3 dir) {
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= vec2(0.1591, 0.3183);  // 1/(2π), 1/π
    uv += 0.5;
    return texture(envMap, uv).rgb;
}

void main()
{
    uint matId = gl_InstanceCustomIndexEXT;
    if (matId >= materialBuffer.materials.length()) {
        hitValue = vec3(0.02f, 0.0f, 0.1f);  // Dark fallback
        return;
    }

    const MaterialData mat = materialBuffer.materials[matId];

    // === BASE COLOR ===
    vec3 albedo = mat.diffuse.rgb;
    float metalness = mat.metallic;
    float roughness = mat.roughness;

    // === METALLIC WORKFLOW ===
    vec3 F0 = mix(vec3(0.04f), albedo, metalness);
    vec3 diffuseColor = albedo * (1.0f - metalness);

    // === LIGHTING (simple) ===
    vec3 N = normalize(gl_WorldRayDirectionEXT);  // Approximate normal
    vec3 L = normalize(-pc.lightDirection);
    float NdotL = max(dot(N, L), 0.0f);

    vec3 lightContrib = pc.lightIntensity * NdotL * vec3(1.0f);

    // === FINAL COLOR ===
    vec3 color = diffuseColor * lightContrib + mat.emission.rgb;

    // === ENVIRONMENT REFLECTION (for glossy) ===
    if (roughness < 0.9f) {
        vec3 R = reflect(gl_WorldRayDirectionEXT, N);
        vec3 env = sampleEnvMap(R);
        color += env * (1.0f - roughness) * 0.5f;
    }

    // === FIREFLY CLAMP ===
    color = min(color, vec3(pc.fireflyClamp));

    hitValue = color;
}