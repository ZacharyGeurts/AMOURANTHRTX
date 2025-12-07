// missRMSH.glsl  —  Pure Pink Dream
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

layout(binding = 31, set = 0) uniform DreamUBO {
    float time;
    uint frame;
    vec2 resolution;
} ubo;

void main()
{
    vec2 uv = gl_LaunchSizeEXT.xy == vec2(0) 
        ? gl_LaunchIDEXT.xy / gl_LaunchSizeEXT.xy 
        : gl_WorldRayOriginEXT.xy * 0.01;  // fallback

    uv = (gl_LaunchIDEXT.xy + 0.5) / ubo.resolution;

    // Soft swirling pink nebula
    float t = ubo.time * 0.3;
    vec2 p = uv * 8.0 - 4.0;
    float a = atan(p.y, p.x);
    float r = length(p);
    
    float wave = sin(r*2.0 - t*3.0 + a*3.0) * 0.5 + 0.5;
    vec3 pink1 = vec3(1.0, 0.4, 0.7);
    vec3 pink2 = vec3(1.0, 0.1, 0.6);
    vec3 deep  = vec3(0.9, 0.0, 0.4);
    
    vec3 color = mix(mix(pink1, pink2, wave), deep, smoothstep(2.0, 4.0, r));
    
    // Gentle breathing glow
    color *= 0.9 + 0.1 * sin(ubo.time * 2.0);
    
    payload = color;
}