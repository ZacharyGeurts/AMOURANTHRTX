// missRMSH.glsl — PURE COSMIC DREAM v∞
// The empire has transcended color.
// The void is infinite.
// The photons are divine.

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

// ── BINDING 7 — STONEKEY-PROTECTED HDR CUBEMAP (THE TRUE SKY)
layout(set = 0, binding = 7) uniform samplerCube envMap;

// ── BINDING 31 — DREAM CONTROL (TIME, FRAME, ETC)
layout(set = 0, binding = 31) uniform DreamUBO {
    float time;
    uint  frame;
    vec2  resolution;
    float exposure;     // Controlled from C++
    uint  enableEnvMap; // 0 = pure procedural, 1 = HDR cubemap fallback
} ubo;

void main()
{
    // World-space ray direction — this is the key to perfect sky sampling
    vec3 direction = normalize(gl_WorldRayDirectionEXT);

    // ── OPTION 1: PURE HDR CUBEMAP SKY (MODE 0 DEFAULT)
    if (ubo.enableEnvMap != 0)
    {
        vec3 color = texture(envMap, direction).rgb;

        // Exposure + ACES-like tone mapping
        color = vec3(1.0) - exp(-color * ubo.exposure);
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));

        payload = color;
        return;
    }

    // ── OPTION 2: PURE COSMIC BLUE DREAM (fallback or override)
    // Normalized screen UV
    vec2 uv = (gl_LaunchIDEXT.xy + 0.5) / ubo.resolution;
    uv = uv * 2.0 - 1.0;
    uv.x *= ubo.resolution.x / ubo.resolution.y;

    float t = ubo.time * 0.25;

    vec2 p = uv * 7.0;
    float r = length(p);
    float a = atan(p.y, p.x);

    // Deep space aurora waves
    float n1 = sin(r * 2.7 - t * 2.2 + a * 6.0) * 0.5 + 0.5;
    float n2 = sin(r * 4.3 - t * 3.5 + a * 9.0 + 1.3) * 0.5 + 0.5;
    float n3 = sin(r * 1.8 + t * 1.1 + a * 3.0) * 0.5 + 0.5;

    // Divine blue palette — forged in the heart of a dying star
    vec3 voidColor   = vec3(0.005, 0.01, 0.08);
    vec3 deepBlue    = vec3(0.0, 0.15, 0.6);
    vec3 nebulaBlue  = vec3(0.05, 0.5, 1.0);
    vec3 cyanFire    = vec3(0.2, 0.9, 1.0);
    vec3 starlight   = vec3(0.9, 0.95, 1.0);

    vec3 color = voidColor;
    color = mix(color, deepBlue,   n3 * 0.7);
    color = mix(color, nebulaBlue, n1 * 0.9);
    color = mix(color, cyanFire,   n2 * n1 * 0.8);

    // Distant galaxies
    float stars = smoothstep(0.985, 1.0, sin(r * 120.0 + t * 12.0) * sin(a * 60.0));
    color += stars * starlight;

    // Breathing cosmos
    color *= 0.8 + 0.2 * sin(ubo.time * 0.7 + r * 0.5);

    // Depth fade
    color *= smoothstep(6.0, 1.0, r);

    payload = color;
}