// filename: closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

hitAttributeEXT vec3 attribs;

layout(set = 0, binding = 4) readonly buffer Materials {
    vec4 colors[];      // simplistic: one vec4 color per instance
} materials;

void main() {
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // very basic normal from barycentrics (for smooth sphere)
    vec3 normal = normalize(barycentrics);

    // get material color (instance index → material index)
    uint matIndex = gl_InstanceCustomIndexEXT;
    vec3 baseColor = materials.colors[matIndex].rgb;

    // simple lambert + ambient
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
    float diff = max(dot(normal, lightDir), 0.0);

    hitColor = baseColor * (0.2 + 0.8 * diff);
}