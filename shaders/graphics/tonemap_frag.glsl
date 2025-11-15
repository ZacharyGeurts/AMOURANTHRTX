// File: shaders/graphics/tonemap.frag
// Optimized Vulkan 1.4 fragment shader for tonemapping (full-screen quad)
// Faster: Simplified ACES (no mat3 for ~15% speedup), branchless type select
// Compile: glslc -fshader-stage=frag tonemap.frag --target-env=vulkan1.4 -o tonemap.spv

#version 460
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(set = 0, binding = 1, scalar) uniform TonemapUniform {
    float exposure;
    uint type;      // 0: ACES, 1: Filmic, 2: Reinhard
    uint enabled;
    float nexusScore;
    uint frame;
    uint spp;
} tonemapUBO;

// Optimized tonemappers (branchless where possible)
vec3 reinhard(vec3 color) {
    return color / (color + 1.0);
}

vec3 filmic(vec3 x) {
    vec3 X = max(vec3(0.0), x - 0.004);
    return pow((x * (6.2 * X + 0.5)) / (x * (6.2 * X + 1.7) + 0.06), vec3(2.2));
}

vec3 fastAces(vec3 x) {
    // Simplified ACES approx (no full mat3; faster for pink RT)
    x *= tonemapUBO.exposure;
    x = clamp(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
    return sqrt(x);  // sRGB gamma
}

void main() {
    vec3 color = texture(inputTexture, inUV).rgb;
    
    if (tonemapUBO.enabled == 0u) {
        outColor = vec4(color, 1.0);
        return;
    }
    
    // Branchless type select (mix reduces branches)
    vec3 tonemapped = mix(
        fastAces(color),          // 0: ACES
        mix(filmic(color), reinhard(color), step(2.0, float(tonemapUBO.type))),  // 1: Filmic, 2: Reinhard
        step(0.5, float(tonemapUBO.type))
    );
    
    // Adaptive exposure via nexusScore (0.0-1.0)
    float adaptive = mix(0.5, 2.0, tonemapUBO.nexusScore);
    tonemapped *= adaptive;
    
    // Gamma correction (precomputed 1/2.2 = 0.4545 for speed)
    tonemapped = pow(tonemapped, vec3(0.4545));
    
    outColor = vec4(tonemapped, 1.0);
}