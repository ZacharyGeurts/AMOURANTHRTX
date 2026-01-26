#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() {
    // Pinkish sky with subtle gradient
    vec3 sky = mix(vec3(0.8, 0.4, 0.6), vec3(0.1, 0.1, 0.3), gl_WorldRayDirectionEXT.y * 0.5 + 0.5);
    hitValue = sky;
}