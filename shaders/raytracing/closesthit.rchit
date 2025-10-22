// assets/shaders/raytracing/closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT float visibility;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3) buffer MaterialSSBO {
    vec4 albedo[];
} materials;
layout(binding = 4) buffer DimensionDataSSBO {
    vec4 dimensions[];
} dimensionData;

layout(push_constant) uniform PushConstants {
    vec4 clearColor;
    vec3 cameraPosition;
    vec3 lightDirection;
    float lightIntensity;
    uint samplesPerPixel;
    uint maxDepth;
    uint maxBounces;
    float russianRoulette;
} push;

hitAttributeEXT vec3 attribs;

void main() {
    // Get barycentric coordinates and instance/triangle indices
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    uint instanceID = gl_InstanceID;
    uint primitiveID = gl_PrimitiveID;

    // Fetch material data (assuming material index is stored or derived)
    vec4 albedo = materials.albedo[gl_InstanceCustomIndexEXT];

    // Compute hit point and normal
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(barycentrics, 0.0)).xyz;

    // Simple Lambertian shading
    vec3 lightDir = normalize(push.lightDirection);
    float NdotL = max(dot(normal, -lightDir), 0.0);

    // Trace shadow ray
    visibility = 1.0;
    traceRayEXT(
        topLevelAS,           // Acceleration structure
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, // Ray flags
        0xFF,                 // Cull mask
        0,                    // SBT offset (shadow hit group)
        0,                    // SBT stride
        1,                    // Miss index (shadow miss shader)
        hitPoint,             // Ray origin
        0.001,                // Tmin
        -lightDir,            // Ray direction
        1000.0,               // Tmax
        1                     // Payload location (visibility)
    );

    // Compute final color
    hitValue = albedo.rgb * NdotL * push.lightIntensity * visibility;

    // Handle bounces (simplified reflection)
    if (gl_HitTEXT < push.maxDepth && push.maxBounces > 0) {
        float rr = push.russianRoulette;
        if (rr < 1.0 && fract(sin(gl_LaunchIDEXT.x * 12.9898 + gl_LaunchIDEXT.y * 78.233) * 43758.5453) > rr) {
            return; // Russian roulette termination
        }

        vec3 reflectDir = reflect(gl_WorldRayDirectionEXT, normal);
        vec3 reflectPayload = vec3(0.0);
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            0,                    // SBT offset (primary hit group)
            0,                    // SBT stride
            0,                    // Miss index (primary miss shader)
            hitPoint,
            0.001,
            reflectDir,
            1000.0,
            0                     // Payload location (hitValue)
        );
        hitValue += reflectPayload * 0.5; // Attenuate reflection contribution
    }
}