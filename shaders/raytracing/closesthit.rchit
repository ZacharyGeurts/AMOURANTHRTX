// closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require

struct Payload {
    vec3 color;
    vec3 attenuation;
    uint depth;
    bool miss;
};

layout(location = 0) rayPayloadInEXT Payload payload;

hitAttributeEXT vec3 hitAttribs;

layout(binding = 2, set = 0) uniform sampler2D envMap;
layout(binding = 3, set = 0) uniform sampler2D snowTex;  // Now used for sparkly snow particles

layout(push_constant) uniform PushConstants {
    mat4 invView;
    mat4 invProj;
    float totalTime;
    uint spp;
    uint frameSeed;
    uint forceEnvOnly;
    float jitterStrength;
    uint maxRecursion;
    uint useEnvSky;
    uint flipEnvV;
    uint showHotPink;
    float environmentExposure;
    float skyIntensity;
    float environmentRotationY;
    vec3 billboardBaseColor;
    float billboardAlphaCutoff;
    uint billboardUseAlphaBlend;
    uint showNormals;
    uint showUVs;
    uint showWireframe;
    uint enableReflections;
    uint enableShadows;
    uint enableVolumetrics;
    float fogPulseSpeed;
    float fogPulseAmount;
    float lightBobSpeed;
    float lightBobAmplitude;
    float lightOrbitSpeed;
    float lightOrbitAmplitude;
    float lightColorPulseSpeed;
    float lightColorPulseAmount;
} pc;

vec3 sampleEnvironment(vec3 dir)
{
    vec2 uv;
    uv.x = atan(dir.z, dir.x) * (0.5 / 3.14159265359) + 0.5 + (pc.environmentRotationY / (2.0 * 3.14159265359));
    uv.y = acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265359;
    if (pc.flipEnvV == 1) uv.y = 1.0 - uv.y;
    uv.x = fract(uv.x);
    return texture(envMap, uv).rgb * pc.environmentExposure * pc.skyIntensity;
}

// Simple hash for per-particle variation
float hash(vec2 p)
{
    p = fract(p * vec2(0.3183099, 0.3678794));
    p = p * p * (3.0 - 2.0 * p);
    return fract(p.x + p.y);
}

void main()
{
    vec3 bary = vec3(1.0 - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);

    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 albedo = vec3(0.15, 0.45, 0.1);  // Base green for mug (fallback)
    float metallic = 0.0;
    float roughness = 0.95;
    vec2 uv = vec2(0.0);

    // Instance 0 = Sparkly Snow Billboard(s)
    if (gl_InstanceCustomIndexEXT == 0)
    {
        uv = bary.x * vec2(0.0, 1.0) +
             bary.y * vec2(0.0, 0.0) +
             bary.z * vec2(1.0, 0.0);

        vec4 tex = texture(snowTex, uv);

        // Base snow is bright white with subtle blue tint
        albedo = vec3(1.0, 1.02, 1.1);  // Very slightly bluish white

        // Use texture brightness as sparkle intensity
        float brightness = dot(tex.rgb, vec3(0.333));
        float sparkleMask = max(brightness - 0.6, 0.0) * 4.0;  // Only bright flakes sparkle

        // Time-based twinkle per particle
        float twinkle = 0.7 + 0.3 * sin(pc.totalTime * 10.0 + hash(uv + gl_PrimitiveID) * 6.28);

        // High metallic + low roughness = sharp, bright specular highlights
        metallic  = 0.85 + sparkleMask * 0.15;   // Extra metallic on bright flakes
        roughness = 0.25 - sparkleMask * 0.15;  // Sharper reflections on sparkles

        // Emissive sparkle (additive glow)
        vec3 emissive = vec3(1.4, 1.5, 1.8) * sparkleMask * twinkle * 2.0;

        // Normal facing camera for flat billboard
        normal = vec3(0.0, 0.0, -1.0);

        // Add emissive directly to final color later
        payload.color = emissive;
    }
    // Ground
    else
    {
        uv = bary.x * vec2(0.0, 0.0) +
             bary.y * vec2(0.0, 1.0) +
             bary.z * vec2(1.0, 1.0);

        albedo = vec3(0.8, 0.85, 0.9);  // Light snowy ground
        metallic  = 0.0;
        roughness = 0.8;
    }

    if (pc.showNormals == 1) {
        payload.color = normal * 0.5 + 0.5;
        payload.attenuation = vec3(0.0);
        payload.depth++;
        return;
    }
    if (pc.showUVs == 1) {
        payload.color = vec3(uv, 0.0);
        payload.attenuation = vec3(0.0);
        payload.depth++;
        return;
    }

    // Base F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Diffuse environment lighting
    vec3 diffuseEnv = sampleEnvironment(normal) * 0.6 + sampleEnvironment(-normal) * 0.3;
    vec3 diffuse = albedo * diffuseEnv * (1.0 - metallic);

    // Specular reflection (strong on snow sparkles)
    vec3 specular = vec3(0.0);
    if (pc.enableReflections == 1) {
        vec3 viewDir = -gl_WorldRayDirectionEXT;
        vec3 reflDir = reflect(gl_WorldRayDirectionEXT, normal);
        vec3 reflColor = sampleEnvironment(reflDir);

        float cosTheta = max(dot(viewDir, normal), 0.0);
        vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);

        float specularStrength = (1.0 - roughness) * (1.0 - roughness);
        specular = reflColor * F * specularStrength * 1.5;  // Brighter specular for snow
    }

    vec3 finalColor = diffuse + specular;

    // Add emissive sparkle (only for snow instance)
    if (gl_InstanceCustomIndexEXT == 0) {
        vec4 tex = texture(snowTex, uv);
        float brightness = dot(tex.rgb, vec3(0.333));
        float sparkleMask = max(brightness - 0.6, 0.0) * 4.0;
        float twinkle = 0.7 + 0.3 * sin(pc.totalTime * 10.0 + hash(uv + gl_PrimitiveID) * 6.28);
        vec3 emissive = vec3(1.4, 1.5, 1.8) * sparkleMask * twinkle * 2.0;
        finalColor += emissive;
    }

    payload.color = finalColor;
    payload.attenuation = vec3(0.0);
    payload.depth++;
}