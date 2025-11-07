// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025
// FULL C++23 TURBO – REDEFINITION-FREE EDITION
// REMOVED: VulkanResourceManager & Vulkan::Context (now ONLY in Dispose.hpp)
// FIXED: GCC 14 utility bug already solved globally in VulkanCommon.hpp
// Zachary Geurts 2025 – "Spinal column, now lean and mean"

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// Forward declarations ONLY
// ===================================================================
class VulkanBufferManager;
class VulkanResourceManager;        // DEFINED IN Dispose.hpp
namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
}

namespace Vulkan {
    struct Context;                // DEFINED IN Dispose.hpp
}

// ===================================================================
// Only function prototypes that belong here
// ===================================================================
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"