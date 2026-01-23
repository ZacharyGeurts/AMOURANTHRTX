#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() {
    // Simple blue-ish sky with sun
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float sun = max(0.0, dot(dir, vec3(0.0, 1.0, 0.0)));
    vec3 sky = mix(vec3(0.05, 0.1, 0.3), vec3(0.6, 0.8, 1.0), dir.y * 0.5 + 0.5);
    sky += vec3(1.0, 0.9, 0.8) * pow(sun, 256.0) * 2.0;

    hitValue = sky * 3.0;
}