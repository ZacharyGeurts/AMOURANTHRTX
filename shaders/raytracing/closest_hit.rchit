#version 460
#extension GL_EXT_ray_tracing : require

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    vec3 normal = normalize(attribs);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normal, lightDir), 0.0);

    vec3 baseColor = vec3(0.9, 0.2, 0.7); // Signature Amouranth pink
    vec3 diffuse = baseColor * (0.1 + 0.9 * NdotL);

    hitValue = diffuse;
}