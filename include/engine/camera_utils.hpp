// include/engine/camera_utils.hpp
// Lazy camera – one static PerspectiveCamera, auto-aspect, no heap after first use
#pragma once
#include "engine/camera.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <cmath>

namespace VulkanRTX {

/// Return a *non-owning* pointer to a default PerspectiveCamera.
/// The object lives for the whole process lifetime.
inline Camera* lazyInitCamera(const ::Vulkan::Context& ctx)
{
    static PerspectiveCamera cam(
        60.0f,
        static_cast<float>(ctx.width) / ctx.height,
        0.1f,
        1000.0f
    );

    const float curAspect = static_cast<float>(ctx.width) / ctx.height;
    if (std::abs(cam.aspectRatio_ - curAspect) > 1e-6f) {
        cam.setAspectRatio(curAspect);
    }

    return &cam;
}

} // namespace VulkanRTX