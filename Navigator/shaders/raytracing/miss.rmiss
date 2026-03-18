// filename: miss.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

void main() {
    // simple gradient sky
    vec3 dir = gl_WorldRayDirectionEXT;
    float t = 0.5 * (dir.y + 1.0);
    hitColor = mix(vec3(0.1, 0.2, 0.8), vec3(0.8, 0.9, 1.0), t);
}