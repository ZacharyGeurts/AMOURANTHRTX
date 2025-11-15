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

// Tonemapping functions
vec3 reinhard(vec3 color) {
    return color / (color + 1.0);
}

vec3 filmic(vec3 x) {
    vec3 X = max(vec3(0.0), x - 0.004);
    vec3 result = (x * (6.2 * X + 0.5)) / (x * (6.2 * X + 1.7) + 0.06);
    return pow(result, vec3(2.2));
}

vec3 aces(vec3 x) {
    const mat3 ACES = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    const mat3 RRT_SAT = mat3(
        0.970889, -0.040963,  0.023983,
        0.029639,  1.000000,  0.000000,
        0.000000,  0.000000,  1.076942
    );
    x *= tonemapUBO.exposure;
    x = ACES * x;
    x = clamp(x, 0.0, 1.0);
    x = RRT_SAT * x;
    x = clamp(x, 0.0, 1.0);
    x = sqrt(x);  // Gamma 0.5 for sRGB
    return x;
}

void main() {
    vec3 color = texture(inputTexture, inUV).rgb;
    
    if (tonemapUBO.enabled == 0u) {
        outColor = vec4(color, 1.0);
        return;
    }
    
    // Apply tonemapping based on type
    if (tonemapUBO.type == 0u) {  // ACES
        color = aces(color);
    } else if (tonemapUBO.type == 1u) {  // Filmic
        color = filmic(color);
    } else if (tonemapUBO.type == 2u) {  // Reinhard
        color = reinhard(color);
    }
    
    // Adaptive exposure tweak via nexusScore (0.0-1.0)
    float adaptiveExposure = mix(0.5, 2.0, tonemapUBO.nexusScore);
    color *= adaptiveExposure;
    
    // Gamma correction for sRGB
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, 1.0);
}