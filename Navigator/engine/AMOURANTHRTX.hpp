#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

// Core includes
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Standard library includes
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Third-party includes
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

// Engine-specific includes
#include "ELLIE.hpp"
#include "OptionsMenu.hpp"

// Vulkan extensions definitions
#define VK_KHR_acceleration_structure 1
#define VK_KHR_ray_tracing_pipeline 1
#define VK_KHR_deferred_host_operations 1
#define VK_KHR_buffer_device_address 1
#define VK_EXT_descriptor_buffer 1
#define VK_EXT_present_mode_fifo_latest_ready 1

// Required Vulkan device extensions (core ones, timing is optional)
inline constexpr std::array<const char*, 8> requiredDeviceExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
}};

// Optional extensions
inline constexpr const char* optionalTimingExtension = VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME;

// Vulkan queue family indices structure
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics, present, compute, transfer;
    [[nodiscard]] bool complete() const noexcept { return graphics && present && compute; }
};

// Function to find queue families
static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) noexcept {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const VkQueueFamilyProperties& f = families[i];
        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        if (f.queueFlags & VK_QUEUE_COMPUTE_BIT)  indices.compute  = i;
        if (surface != VK_NULL_HANDLE) {
            VkBool32 supp = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &supp);
            if (supp) indices.present = i;
        }
        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.transfer = i;
    }

    if (!indices.compute.has_value()) indices.compute = indices.graphics;
    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;

    return indices;
}

// Create Vulkan instance
[[nodiscard]] inline VkInstance createVulkanInstance() noexcept {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 81, 0);
    appInfo.pEngineName        = "VALHALLA";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 0, 81, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    uint32_t sdlCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);

    std::vector<const char*> extensions(sdlExts, sdlExts + sdlCount);
    std::vector<const char*> layers;

    if (Options::Debug::ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkInstance inst = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&ci, nullptr, &inst);
    vkh.checker(res, "vkCreateInstance", "vkCreateInstance failed");

    LOG_SUCCESS_CAT("VULKAN", "Instance created — {} extensions, validation {}",
                    extensions.size(), Options::Debug::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF");
    return inst;
}

// Geometry type enum
enum class GeometryType : uint32_t {
    ProceduralPlane     = 0,
    ProceduralSphere    = 1,
    ProceduralCylinder  = 2,
    ProceduralCone      = 3,
    ProceduralD4        = 4,
    ProceduralD6        = 5,
    ProceduralD8        = 6,
    ProceduralD10       = 7,
    ProceduralD12       = 8,
    ProceduralD20       = 9,
    ProceduralD100      = 10,
};

// Universal primitive
struct UniversalPrimitive {
    glm::vec4 aabbMin;
    glm::vec4 aabbMax;
    glm::mat4 transform;
    uint32_t type          = 0;
    uint32_t materialIndex = 0;
    float destruction      = 0.0f;
};

// Buffer info
struct BufferInfo {
    VkBuffer           buffer        = VK_NULL_HANDLE;
    VkDeviceMemory     memory        = VK_NULL_HANDLE;
    VkDeviceSize       size          = 0;
    VkDeviceSize       aligned       = 0;
    VkDeviceSize       offset        = 0;
    VkDeviceAddress    deviceAddress = 0;
    void*              mapped        = nullptr;
    VkBufferUsageFlags usage         = 0;
    std::string        tag;
};

// VRAM reality
struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = 256ULL << 20;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
    VkDeviceSize max_alloc_size   = 0;
    uint32_t max_alloc_count      = 0;
};

// Global RTX context
struct RTX {
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    VkQueue transfer_queue = VK_NULL_HANDLE;

    uint32_t graphics_family = ~0u;
    uint32_t present_family = ~0u;
    uint32_t transfer_family = ~0u;
    uint32_t compute_family = ~0u;

    SDL_Window* window = nullptr;

    VkImage images = VK_NULL_HANDLE;
    VkImageView views = VK_NULL_HANDLE;
    VkExtent2D extent{};
    uint32_t image_count = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
    bool rt_props_cached = false;
    VkCommandPool transient_pool = VK_NULL_HANDLE;

    VkPipeline compute_pipeline = VK_NULL_HANDLE;
    VkPipeline rt_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    uint64_t living_world_buffer_handle = 0;
    uint64_t descriptor_buffer_handle = 0;
    void* descriptor_mapped = nullptr;

    VkDeviceAddress descriptor_buffer_address = 0;
    std::array<VkDeviceSize, 9> binding_offsets{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_props{};
    bool descriptor_props_cached = false;

    bool eternal_sbt_forged = false;
    VkDeviceAddress sbt_address = 0;
    VkDeviceSize sbt_size = 0;
    VkStridedDeviceAddressRegionKHR raygen_sbt_region{};
    VkStridedDeviceAddressRegionKHR miss_sbt_region{};
    VkStridedDeviceAddressRegionKHR hit_sbt_region{};

    VkAccelerationStructureKHR dummy_tlas = VK_NULL_HANDLE;
    VkBuffer dummy_accel_buffer = VK_NULL_HANDLE;
    VkDeviceMemory dummy_accel_memory = VK_NULL_HANDLE;

    VkDescriptorSetLayout main_descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout tex_descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout empty_descriptor_layout = VK_NULL_HANDLE;

    uint32_t raygen_group_count = 0;
    uint32_t miss_group_count = 0;
    uint32_t hit_group_count = 0;

    uint64_t las_instance_buffer = 0;
    uint64_t las_universal_primitives_buffer = 0;
    VkAccelerationStructureKHR las_tlas = VK_NULL_HANDLE;
    uint64_t las_tlas_storage = 0;

    std::vector<UniversalPrimitive> las_procedural_primitives;

    bool las_initialized = false;
    bool las_tlas_dirty = true;
    bool las_procedural_dirty = true;

    std::unordered_map<uint64_t, BufferInfo> buffers;
    uint64_t next_buffer_handle = 0x00000001ULL;
    std::mutex buffer_mutex;

    VRAMReality vram_reality{};
};

inline RTX& rtx() noexcept {
    static RTX e;
    return e;
}

// Vulkan extension function pointers singleton
struct VulkanExtensions {
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;

    PFN_vkCreateSwapchainKHR                      vkCreateSwapchainKHR                      = nullptr;
    PFN_vkDestroySwapchainKHR                     vkDestroySwapchainKHR                     = nullptr;
    PFN_vkGetSwapchainImagesKHR                   vkGetSwapchainImagesKHR                   = nullptr;
    PFN_vkAcquireNextImageKHR                     vkAcquireNextImageKHR                     = nullptr;
    PFN_vkQueuePresentKHR                         vkQueuePresentKHR                         = nullptr;

    PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR            = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR      = nullptr;
    PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR                         = nullptr;

    PFN_vkGetAccelerationStructureBuildSizesKHR   vkGetAccelerationStructureBuildSizesKHR   = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR       = nullptr;
    PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR          = nullptr;
    PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR         = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    PFN_vkGetBufferDeviceAddress                  vkGetBufferDeviceAddress                  = nullptr;

    PFN_vkCmdCopyAccelerationStructureKHR         vkCmdCopyAccelerationStructureKHR         = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCmdTraceRaysIndirect2KHR                vkCmdTraceRaysIndirect2KHR                = nullptr;

    PFN_vkCmdBeginRendering                       vkCmdBeginRendering                       = nullptr;
    PFN_vkCmdEndRendering                         vkCmdEndRendering                         = nullptr;
    PFN_vkCmdPipelineBarrier2                     vkCmdPipelineBarrier2                     = nullptr;
    PFN_vkQueueSubmit2KHR                         vkQueueSubmit2KHR                         = nullptr;

    PFN_vkSetDebugUtilsObjectNameEXT              vkSetDebugUtilsObjectNameEXT              = nullptr;

    PFN_vkGetDescriptorSetLayoutSizeEXT           vkGetDescriptorSetLayoutSizeEXT           = nullptr;
    PFN_vkGetDescriptorSetLayoutBindingOffsetEXT  vkGetDescriptorSetLayoutBindingOffsetEXT  = nullptr;
    PFN_vkCmdBindDescriptorBuffersEXT             vkCmdBindDescriptorBuffersEXT             = nullptr;
    PFN_vkCmdSetDescriptorBufferOffsetsEXT        vkCmdSetDescriptorBufferOffsetsEXT        = nullptr;
    PFN_vkGetDescriptorEXT                        vkGetDescriptorEXT                        = nullptr;

    PFN_vkGetRefreshCycleDurationGOOGLE           vkGetRefreshCycleDurationGOOGLE           = nullptr;
    PFN_vkGetPastPresentationTimingGOOGLE         vkGetPastPresentationTimingGOOGLE         = nullptr;
};

inline VulkanExtensions& ext() noexcept {
    static VulkanExtensions e;
    static bool loaded = false;

    if (loaded) return e;

    VkInstance inst = rtx().instance;
    if (inst == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("EXT", "Instance not created");
        return e;
    }

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());

    if (!vkGetInstanceProcAddr) {
        LOG_ERROR_CAT("EXT", "Failed to get vkGetInstanceProcAddr from SDL");
        return e;
    }

    e.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    e.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    e.vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    e.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));

    VkDevice dev = rtx().device;
    if (dev == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("EXT", "Device not created — only instance extensions loaded");
        loaded = true;
        return e;
    }

    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(inst, "vkGetDeviceProcAddr"));

    if (!vkGetDeviceProcAddr) {
        LOG_ERROR_CAT("EXT", "Failed to get vkGetDeviceProcAddr");
        return e;
    }

    e.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
    e.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
    e.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
    e.vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
    e.vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));

    e.vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(dev, "vkCreateRayTracingPipelinesKHR"));
    e.vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(dev, "vkGetRayTracingShaderGroupHandlesKHR"));
    e.vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(dev, "vkCmdTraceRaysKHR"));

    e.vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureBuildSizesKHR"));
    e.vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(dev, "vkCmdBuildAccelerationStructuresKHR"));
    e.vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(dev, "vkCreateAccelerationStructureKHR"));
    e.vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(dev, "vkDestroyAccelerationStructureKHR"));
    e.vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureDeviceAddressKHR"));

    e.vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(dev, "vkGetBufferDeviceAddress"));

    e.vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(dev, "vkCmdCopyAccelerationStructureKHR"));
    e.vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(dev, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    e.vkCmdTraceRaysIndirect2KHR = reinterpret_cast<PFN_vkCmdTraceRaysIndirect2KHR>(vkGetDeviceProcAddr(dev, "vkCmdTraceRaysIndirect2KHR"));

    e.vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(vkGetDeviceProcAddr(dev, "vkCmdBeginRendering"));
    e.vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(vkGetDeviceProcAddr(dev, "vkCmdEndRendering"));
    e.vkCmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(vkGetDeviceProcAddr(dev, "vkCmdPipelineBarrier2"));
    e.vkQueueSubmit2KHR = reinterpret_cast<PFN_vkQueueSubmit2KHR>(vkGetDeviceProcAddr(dev, "vkQueueSubmit2KHR"));

    e.vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(dev, "vkSetDebugUtilsObjectNameEXT"));

    e.vkGetDescriptorSetLayoutSizeEXT = reinterpret_cast<PFN_vkGetDescriptorSetLayoutSizeEXT>(vkGetDeviceProcAddr(dev, "vkGetDescriptorSetLayoutSizeEXT"));
    e.vkGetDescriptorSetLayoutBindingOffsetEXT = reinterpret_cast<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>(vkGetDeviceProcAddr(dev, "vkGetDescriptorSetLayoutBindingOffsetEXT"));
    e.vkCmdBindDescriptorBuffersEXT = reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(vkGetDeviceProcAddr(dev, "vkCmdBindDescriptorBuffersEXT"));
    e.vkCmdSetDescriptorBufferOffsetsEXT = reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(vkGetDeviceProcAddr(dev, "vkCmdSetDescriptorBufferOffsetsEXT"));
    e.vkGetDescriptorEXT = reinterpret_cast<PFN_vkGetDescriptorEXT>(vkGetDeviceProcAddr(dev, "vkGetDescriptorEXT"));

    e.vkGetRefreshCycleDurationGOOGLE = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(vkGetDeviceProcAddr(dev, "vkGetRefreshCycleDurationGOOGLE"));
    e.vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(vkGetDeviceProcAddr(dev, "vkGetPastPresentationTimingGOOGLE"));

    loaded = true;
    LOG_SUCCESS_CAT("EXT", "All Vulkan extensions loaded successfully");

    return e;
}

// Memory namespace — unified buffer creation with auto-detection
namespace Memory {

inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE = 256ULL << 20;
inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 1ULL << 20; // 1 MiB

enum class MemoryHint : uint8_t {
    Auto              = 0,
    DeviceLocalOnly   = 1,
    HostVisible       = 2,
    DescriptorBuffer  = 3
};

[[nodiscard]] constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred = 0) noexcept {
    VkPhysicalDevice phys = rtx().physical;
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

    uint32_t best = ~0u;
    int bestScore = -1;

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) == 0) continue;
        VkMemoryPropertyFlags flags = memProps.memoryTypes[i].propertyFlags;
        if ((flags & required) != required) continue;

        int score = 0;
        if (flags & preferred) score += 10;
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) score += 5;

        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }

    if (best == ~0u) LOG_ERROR_CAT("MEMORY", "No memory type for required 0x{:x}", required);
    return best;
}

[[nodiscard]] inline VRAMReality measureReality() noexcept {
    static VRAMReality reality{};
    static bool measured = false;

    if (measured) return reality;

    VkPhysicalDevice phys = rtx().physical;
    if (phys == VK_NULL_HANDLE) return reality;

    VkPhysicalDeviceMemoryProperties2 memProps2{};
    memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    memProps2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(phys, &memProps2);

    const auto& mem = memProps2.memoryProperties;

    reality.total = 0;
    reality.driver_footprint = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.total += mem.memoryHeaps[i].size;
            reality.driver_footprint += budget.heapUsage[i];
        }
    }

    if (reality.driver_footprint == 0) reality.driver_footprint = 1'500'000'000ULL;

    reality.usable = reality.total > (reality.driver_footprint + reality.safety_margin) ?
                     reality.total - reality.driver_footprint - reality.safety_margin : 0;
    reality.remaining = reality.usable;

    measured = true;
    return reality;
}

[[nodiscard]] inline uint64_t createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    std::string_view tag = "",
    MemoryHint hint = MemoryHint::Auto
) noexcept {
    VkDevice dev = rtx().device;
    if (dev == VK_NULL_HANDLE || size == 0) return 0;

    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)) {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    bool isDescriptor = hint == MemoryHint::DescriptorBuffer ||
                        (usage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
    if (isDescriptor) {
        usage |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    bool isSBT = (usage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) || tag.find("SBT") != std::string_view::npos;
    if (isSBT) {
        size = std::max(size, VkDeviceSize(512));
        size = align_up(size, VkDeviceSize(256));
        usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bool wantHostVisible = hint == MemoryHint::HostVisible ||
                           (hint == MemoryHint::Auto && (
                               size <= HOST_VISIBLE_THRESHOLD ||
                               (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) ||
                               tag.find("camera") != std::string_view::npos ||
                               tag.find("ubo") != std::string_view::npos ||
                               tag.find("material") != std::string_view::npos ||
                               tag.find("param") != std::string_view::npos));

    VkMemoryPropertyFlags required = wantHostVisible ?
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) :
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkMemoryPropertyFlags preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(dev, &bci, nullptr, &buffer) != VK_SUCCESS) return 0;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, required, preferred);
    if (memType == ~0u && wantHostVisible) {
        // Explicit fallback: host-visible + coherent only (no device-local preference)
        VkMemoryPropertyFlags fallbackProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        memType = findMemoryType(req.memoryTypeBits, fallbackProps);
    }
    if (memType == ~0u) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return 0;
    }

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(dev, &mai, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return 0;
    }

    if (vkBindBufferMemory(dev, buffer, memory, 0) != VK_SUCCESS) {
        vkFreeMemory(dev, memory, nullptr);
        vkDestroyBuffer(dev, buffer, nullptr);
        return 0;
    }

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    void* mapped = nullptr;
    if (wantHostVisible) {
        if (vkMapMemory(dev, memory, 0, size, 0, &mapped) != VK_SUCCESS) mapped = nullptr;
    }

    uint64_t handle = ++rtx().next_buffer_handle;
    rtx().buffers.emplace(handle, BufferInfo{
        buffer, memory, size, req.alignment, 0, addr, mapped, usage, std::string(tag)
    });

    LOG_SUCCESS_CAT("MEMORY", "Buffer '{}' created — handle={:016x}, size={} B, {}mappable",
                    tag.empty() ? "untagged" : tag.data(), handle, size, mapped ? "persistently " : "not ");

    return handle;
}

inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    auto it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) return;

    const auto& info = it->second;
    if (info.mapped) vkUnmapMemory(rtx().device, info.memory);
    vkDestroyBuffer(rtx().device, info.buffer, nullptr);
    vkFreeMemory(rtx().device, info.memory, nullptr);
    rtx().buffers.erase(it);
}

[[nodiscard]] inline BufferInfo* get(uint64_t handle) noexcept {
    auto it = rtx().buffers.find(handle);
    return it != rtx().buffers.end() ? &it->second : nullptr;
}

[[nodiscard]] inline VkBuffer getBuffer(uint64_t handle) noexcept {
    if (auto* info = get(handle)) return info->buffer;
    return VK_NULL_HANDLE;
}

[[nodiscard]] inline VkDeviceAddress getDeviceAddress(uint64_t handle) noexcept {
    if (auto* info = get(handle)) return info->deviceAddress;
    return 0;
}

[[nodiscard]] inline void* getMappedPtr(uint64_t handle) noexcept {
    if (auto* info = get(handle)) return info->mapped;
    return nullptr;
}

[[nodiscard]] inline std::pair<VkBuffer, VkDeviceMemory> uploadToBuffer(
    uint64_t handle,
    const void* data,
    VkDeviceSize size,
    VkCommandBuffer cmd = VK_NULL_HANDLE
) noexcept {
    auto* info = get(handle);
    if (!info || size > info->size) return {VK_NULL_HANDLE, VK_NULL_HANDLE};

    if (info->mapped) {
        std::memcpy(info->mapped, data, size);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    VkDevice dev = rtx().device;

    VkDeviceSize stageSize = align_up(size, 256);

    VkBufferCreateInfo stageCI{};
    stageCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stageCI.size = stageSize;
    stageCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stageBuf = VK_NULL_HANDLE;
    if (vkCreateBuffer(dev, &stageCI, nullptr, &stageBuf) != VK_SUCCESS) return {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkMemoryRequirements stageReq{};
    vkGetBufferMemoryRequirements(dev, stageBuf, &stageReq);

    uint32_t stageMemType = findMemoryType(stageReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (stageMemType == ~0u) {
        vkDestroyBuffer(dev, stageBuf, nullptr);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    VkMemoryAllocateInfo stageMai{};
    stageMai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stageMai.allocationSize = stageReq.size;
    stageMai.memoryTypeIndex = stageMemType;

    VkDeviceMemory stageMem = VK_NULL_HANDLE;
    if (vkAllocateMemory(dev, &stageMai, nullptr, &stageMem) != VK_SUCCESS) {
        vkDestroyBuffer(dev, stageBuf, nullptr);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    if (vkBindBufferMemory(dev, stageBuf, stageMem, 0) != VK_SUCCESS) {
        vkFreeMemory(dev, stageMem, nullptr);
        vkDestroyBuffer(dev, stageBuf, nullptr);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    void* ptr = nullptr;
    if (vkMapMemory(dev, stageMem, 0, stageSize, 0, &ptr) != VK_SUCCESS) {
        vkFreeMemory(dev, stageMem, nullptr);
        vkDestroyBuffer(dev, stageBuf, nullptr);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    std::memcpy(ptr, data, size);
    vkUnmapMemory(dev, stageMem);

    bool ownsCmd = (cmd == VK_NULL_HANDLE);
    VkCommandBuffer targetCmd = cmd;

    if (ownsCmd) {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = rtx().transient_pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkAllocateCommandBuffers(dev, &alloc, &targetCmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(targetCmd, &begin);
    }

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = info->offset;
    copy.size = size;

    vkCmdCopyBuffer(targetCmd, stageBuf, info->buffer, 1, &copy);

    if (ownsCmd) {
        vkEndCommandBuffer(targetCmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &targetCmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(rtx().graphics_queue);

        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &targetCmd);
        vkDestroyBuffer(dev, stageBuf, nullptr);
        vkFreeMemory(dev, stageMem, nullptr);

        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    return {stageBuf, stageMem};
}

[[nodiscard]] inline VkDeviceAddress allocateScratch(VkDeviceSize requiredSize) noexcept {
    if (requiredSize == 0) return 0;

    VkDeviceSize total = 0;
    VkDeviceAddress base = 0;

    while (total < requiredSize) {
        VkDeviceSize chunk = std::min(DEFAULT_CHUNK_SIZE, requiredSize - total);
        uint64_t handle = createBuffer(chunk,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       "Scratch");

        if (handle == 0) return 0;

        auto* info = get(handle);
        if (!info) return 0;

        VkDeviceAddress addr = info->deviceAddress;
        if (total == 0) base = addr;

        total += chunk;
    }

    return base;
}

inline void init() noexcept {
    rtx().vram_reality = measureReality();
    LOG_SUCCESS_CAT("MEMORY", "Memory subsystem initialized");
}

} // namespace Memory

// Logical device creation and GPU selection
[[nodiscard]] inline VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance inst,
    VkSurfaceKHR surf,
    uint32_t* out_graphics_family  = nullptr,
    uint32_t* out_present_family   = nullptr,
    uint32_t* out_compute_family   = nullptr,
    uint32_t* out_transfer_family  = nullptr
) noexcept {
    uint32_t count = 0;
    vkh.checker(vkEnumeratePhysicalDevices(inst, &count, nullptr), "vkEnumeratePhysicalDevices (count)", "Failed");

    vkh.checker(count > 0, "Physical devices", "No Vulkan devices found");

    std::vector<VkPhysicalDevice> devices(count);
    vkh.checker(vkEnumeratePhysicalDevices(inst, &count, devices.data()), "vkEnumeratePhysicalDevices (list)", "Failed");

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices best_indices;
    int best_score = -1;
    VkDeviceSize best_memory = 0;

    for (VkPhysicalDevice pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        if (props.apiVersion < VK_API_VERSION_1_3) continue;

        QueueFamilyIndices indices = findQueueFamilies(pd, surf);
        if (!indices.complete()) continue;

        uint32_t extCount = 0;
        vkh.checker(vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr), "Extension count", "Failed");

        std::vector<VkExtensionProperties> exts(extCount);
        vkh.checker(vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data()), "Extension list", "Failed");

        bool has_all_required = true;
        for (const char* req : requiredDeviceExtensions) {
            bool found = false;
            for (const auto& avail : exts) {
                if (strcmp(avail.extensionName, req) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                has_all_required = false;
                break;
            }
        }
        if (!has_all_required) continue;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 100000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 10000;

        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
        VkDeviceSize local_mem = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                local_mem += memProps.memoryHeaps[i].size;
            }
        }
        score += static_cast<int>(local_mem / (1024ULL * 1024 * 1024));

        if (score > best_score || (score == best_score && local_mem > best_memory)) {
            best_score = score;
            best_memory = local_mem;
            selected = pd;
            best_indices = indices;
        }
    }

    vkh.checker(selected != VK_NULL_HANDLE, "GPU selection", "No suitable GPU found");

    rtx().physical = selected;

    std::set<uint32_t> unique_families = {best_indices.graphics.value(), best_indices.present.value(), best_indices.compute.value()};
    if (best_indices.transfer.has_value()) unique_families.insert(best_indices.transfer.value());

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(unique_families.size());
    for (uint32_t fam : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queue_infos.push_back(qci);
    }

    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress = VK_TRUE;
    vk12.descriptorIndexing = VK_TRUE;
    vk12.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
    accel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel.accelerationStructure = VK_TRUE;
    accel.pNext = &vk12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipe{};
    rt_pipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rt_pipe.rayTracingPipeline = VK_TRUE;
    rt_pipe.pNext = &accel;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT desc_buf{};
    desc_buf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    desc_buf.descriptorBuffer = VK_TRUE;
    desc_buf.pNext = &rt_pipe;

    std::vector<const char*> enabledExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

    // Check and enable optional timing extension if supported
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(selected, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(selected, nullptr, &extCount, exts.data());

    bool timingSupported = false;
    for (const auto& avail : exts) {
        if (strcmp(avail.extensionName, optionalTimingExtension) == 0) {
            enabledExtensions.push_back(optionalTimingExtension);
            timingSupported = true;
            break;
        }
    }

    VkDeviceCreateInfo dev_ci{};
    dev_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_ci.pNext = &desc_buf;
    dev_ci.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    dev_ci.pQueueCreateInfos = queue_infos.data();
    dev_ci.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    dev_ci.ppEnabledExtensionNames = enabledExtensions.data();

    VkDevice dev = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(selected, &dev_ci, nullptr, &dev);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "vkCreateDevice failed: {}", vkh.result(res));
        return VK_NULL_HANDLE;
    }

    rtx().device = dev;

    rtx().graphics_family = best_indices.graphics.value();
    rtx().present_family = best_indices.present.value();
    rtx().compute_family = best_indices.compute.value();
    rtx().transfer_family = best_indices.transfer.value_or(best_indices.graphics.value());

    vkGetDeviceQueue(dev, rtx().graphics_family, 0, &rtx().graphics_queue);
    vkGetDeviceQueue(dev, rtx().present_family, 0, &rtx().present_queue);
    vkGetDeviceQueue(dev, rtx().compute_family, 0, &rtx().compute_queue);
    vkGetDeviceQueue(dev, rtx().transfer_family, 0, &rtx().transfer_queue);

    if (out_graphics_family) *out_graphics_family = rtx().graphics_family;
    if (out_present_family) *out_present_family = rtx().present_family;
    if (out_compute_family) *out_compute_family = rtx().compute_family;
    if (out_transfer_family) *out_transfer_family = rtx().transfer_family;

    LOG_SUCCESS_CAT("VULKAN", "Logical device created");

    Memory::init();

    return dev;
}

// Swapchain — single-image, timing controlled by TotalTime only
struct Swapchain {
    struct Handle {
        VkSwapchainKHR value;
        Handle() : value(VK_NULL_HANDLE) {}
        explicit Handle(VkSwapchainKHR v) : value(v) {}
        VkSwapchainKHR get() const noexcept { return value; }
        void reset() noexcept { value = VK_NULL_HANDLE; }
        bool valid() const noexcept { return value != VK_NULL_HANDLE; }
        operator VkSwapchainKHR() const noexcept { return value; }
    };

    inline static Handle swapchain;
    inline static VkImage image = VK_NULL_HANDLE;
    inline static VkImageView view = VK_NULL_HANDLE;
    inline static VkExtent2D extent{};
    inline static VkFormat format{};
    inline static bool minimized = false;
    inline static bool supportsStorage = false;

    inline static double lastPresentTime_s = 0.0;
    inline static double smoothedRefresh_s = 1.0 / 60.0;

    static void createOrRecreate(int width, int height, bool isRecreate = false) noexcept {
        if (width <= 0 || height <= 0) {
            minimized = true;
            return;
        }

        VkDevice dev = rtx().device;
        if (dev == VK_NULL_HANDLE) return;

        vkDeviceWaitIdle(dev);
        vkQueueWaitIdle(rtx().present_queue);

        if (isRecreate) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(dev, view, nullptr);
                view = VK_NULL_HANDLE;
            }
            if (swapchain.valid()) {
                ext().vkDestroySwapchainKHR(dev, swapchain.get(), nullptr);
                swapchain.reset();
            }
            image = VK_NULL_HANDLE;
            rtx().images = VK_NULL_HANDLE;
            rtx().views = VK_NULL_HANDLE;
            minimized = false;
        }

        VkSurfaceCapabilitiesKHR caps{};
        if (ext().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rtx().physical, rtx().surface, &caps) != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Failed to get surface capabilities");
            minimized = true;
            return;
        }

        VkExtent2D newExtent = caps.currentExtent;
        if (newExtent.width == UINT32_MAX) {
            newExtent.width  = std::clamp(static_cast<uint32_t>(width), caps.minImageExtent.width, caps.maxImageExtent.width);
            newExtent.height = std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        if (newExtent.width == 0 || newExtent.height == 0) {
            minimized = true;
            return;
        }

        extent = newExtent;

        uint32_t fmtCount = 0;
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, formats.data());

        VkSurfaceFormatKHR chosenFmt = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];

        constexpr std::array preferred = {
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
        };

        for (const auto& pref : preferred) {
            auto it = std::find_if(formats.begin(), formats.end(),
                                   [&](const auto& f) { return f.format == pref.format && f.colorSpace == pref.colorSpace; });
            if (it != formats.end()) {
                chosenFmt = *it;
                break;
            }
        }

        format = chosenFmt.format;

        uint32_t pmCount = 0;
        ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, nullptr);
        std::vector<VkPresentModeKHR> modes(pmCount);
        ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, modes.data());

        VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;

        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
            chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
        } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
            chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
        }

        uint32_t imgCount = 2;
        imgCount = std::max(caps.minImageCount, imgCount);
        imgCount = std::min(caps.maxImageCount ? caps.maxImageCount : imgCount, imgCount);

        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        supportsStorage = false;

        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
            VkImageFormatProperties props{};
            if (vkGetPhysicalDeviceImageFormatProperties(rtx().physical, format, VK_IMAGE_TYPE_2D,
                                                         VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT, 0, &props) == VK_SUCCESS) {
                supportsStorage = true;
                usage |= VK_IMAGE_USAGE_STORAGE_BIT;
                LOG_SUCCESS_CAT("SWAPCHAIN", "Storage usage enabled — direct ray write possible");
            }
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface         = rtx().surface;
        ci.minImageCount   = imgCount;
        ci.imageFormat     = format;
        ci.imageColorSpace = chosenFmt.colorSpace;
        ci.imageExtent     = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage      = usage;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform    = caps.currentTransform;
        ci.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode     = chosenPM;
        ci.clipped         = VK_TRUE;
        ci.oldSwapchain    = swapchain;

        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        if (ext().vkCreateSwapchainKHR(dev, &ci, nullptr, &newSwap) != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "vkCreateSwapchainKHR failed");
            minimized = true;
            return;
        }

        swapchain.value = newSwap;

        uint32_t actualCount = 0;
        ext().vkGetSwapchainImagesKHR(dev, newSwap, &actualCount, nullptr);
        std::vector<VkImage> images(actualCount);
        ext().vkGetSwapchainImagesKHR(dev, newSwap, &actualCount, images.data());

        image = images[0];
        rtx().images = image;

        VkImageViewCreateInfo viewCI{};
        viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image            = image;
        viewCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format           = format;
        viewCI.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (vkCreateImageView(dev, &viewCI, nullptr, &view) != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "vkCreateImageView failed");
            minimized = true;
            return;
        }

        rtx().views = view;

        lastPresentTime_s = 0.0;
        smoothedRefresh_s = 1.0 / 60.0;

        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain {} — {}x{}, format {}, mode {}, storage={}",
                        isRecreate ? "recreated" : "created",
                        extent.width, extent.height,
                        vkh.format(format),
                        chosenPM == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
                        chosenPM == VK_PRESENT_MODE_MAILBOX_KHR   ? "MAILBOX" : "FIFO",
                        supportsStorage ? "yes" : "no");
    }

    static bool shouldPresentNow() noexcept {
        double now = TotalTime::get().seconds();

        if (lastPresentTime_s <= 0.0) {
            lastPresentTime_s = now;
            return true;
        }

        double next = lastPresentTime_s + smoothedRefresh_s;
        if (now >= next - 0.0005) {
            lastPresentTime_s = now;
            return true;
        }

        return false;
    }

    static void updateRefreshEstimate(double presentTime) noexcept {
        if (lastPresentTime_s > 0.0) {
            double delta = presentTime - lastPresentTime_s;
            if (delta > 0.001 && delta < 0.1) {
                constexpr double alpha = 0.25;
                smoothedRefresh_s = alpha * delta + (1.0 - alpha) * smoothedRefresh_s;
            }
        }
        lastPresentTime_s = presentTime;
    }

    static VkResult tryPresent(VkQueue queue) noexcept {
        if (minimized || !swapchain.valid()) return VK_SUCCESS;

        double now = TotalTime::get().seconds();

        if (!shouldPresentNow()) return VK_EVENT_SET;

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain.value;
        uint32_t idx = 0;
        pi.pImageIndices = &idx;

        VkResult res = ext().vkQueuePresentKHR(queue, &pi);

        if (res == VK_SUCCESS) {
            updateRefreshEstimate(now);
            LOG_INFO_CAT("SWAPCHAIN", "Presented at {:.6f} s (Δ {:.4f}s, ~{:.0f} Hz)",
                         now, smoothedRefresh_s, 1.0 / smoothedRefresh_s);
        } else if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            minimized = true;
            LOG_WARNING_CAT("SWAPCHAIN", "Present out-of-date/suboptimal — minimized");
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(res));
        }

        return res;
    }

    static void recreate(int w, int h) noexcept {
        vkDeviceWaitIdle(rtx().device);
        createOrRecreate(w, h, true);
    }

    static void create(SDL_Window* win, int w, int h) noexcept {
        rtx().window = win;
        createOrRecreate(w, h, false);
    }

    static void cleanup() noexcept {
        VkDevice dev = rtx().device;
        if (dev == VK_NULL_HANDLE) return;

        vkDeviceWaitIdle(dev);
        vkQueueWaitIdle(rtx().present_queue);

        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        if (swapchain.valid()) {
            ext().vkDestroySwapchainKHR(dev, swapchain.get(), nullptr);
            swapchain.reset();
        }

        image = VK_NULL_HANDLE;
        lastPresentTime_s = 0.0;
        smoothedRefresh_s = 1.0 / 60.0;
    }

    static bool isMinimized() noexcept { return minimized; }
    static bool canDirectWrite() noexcept { return supportsStorage; }
    static VkExtent2D getExtent() noexcept { return extent; }
    static VkFormat getFormat() noexcept { return format; }
    static VkImage getImage() noexcept { return image; }
    static VkImageView getView() noexcept { return view; }
    static VkSwapchainKHR get() noexcept { return swapchain.value; }
};

// Vertex structure
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Mesh structure
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 aabbMin{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 aabbMax{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    void computeAABB() noexcept {
        if (vertices.empty()) return;

        aabbMin = vertices[0].pos;
        aabbMax = vertices[0].pos;

        for (const auto& v : vertices) {
            aabbMin = glm::min(aabbMin, v.pos);
            aabbMax = glm::max(aabbMax, v.pos);
        }

        glm::vec3 padding = (aabbMax - aabbMin) * 0.001f;
        aabbMin -= padding;
        aabbMax += padding;
    }
};

// Load OBJ mesh — extract AABB only
[[nodiscard]] inline std::unique_ptr<Mesh> loadOBJ(std::string_view path) noexcept {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string baseDir = "assets/models/";
    bool loaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, std::string(path).c_str(), baseDir.c_str());
    vkh.checker(loaded, "tinyobj::LoadObj", "Failed to load OBJ");

    if (!warn.empty()) LOG_WARNING_CAT("LAS", "{}", warn);

    auto mesh = std::make_unique<Mesh>();

    glm::vec3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            if (index.vertex_index < 0) continue;
            size_t base = static_cast<size_t>(3 * index.vertex_index);
            glm::vec3 pos{attrib.vertices[base], attrib.vertices[base+1], attrib.vertices[base+2]};
            min = glm::min(min, pos);
            max = glm::max(max, pos);
        }
    }

    mesh->aabbMin = min;
    mesh->aabbMax = max;

    glm::vec3 padding = (max - min) * 0.001f;
    mesh->aabbMin -= padding;
    mesh->aabbMax += padding;

    LOG_SUCCESS_CAT("LAS", "OBJ AABB loaded — min: ({:.2f},{:.2f},{:.2f}) max: ({:.2f},{:.2f},{:.2f})", min.x, min.y, min.z, max.x, max.y, max.z);

    return mesh;
}

// Create plane AABB
[[nodiscard]] inline std::unique_ptr<Mesh> createPlane(float width = 1000.0f, float depth = 1000.0f) noexcept {
    auto mesh = std::make_unique<Mesh>();
    const float hw = width * 0.5f;
    const float hd = depth * 0.5f;
    mesh->aabbMin = {-hw, -0.01f, -hd};
    mesh->aabbMax = { hw,  0.01f,  hd};
    LOG_SUCCESS_CAT("LAS", "Plane AABB created — {}x{}", width, depth);
    return mesh;
}

// Create billboard AABB
[[nodiscard]] inline std::unique_ptr<Mesh> createBillboard() noexcept {
    auto mesh = std::make_unique<Mesh>();
    mesh->aabbMin = {-0.5f, -0.5f, -0.01f};
    mesh->aabbMax = { 0.5f,  0.5f,  0.01f};
    LOG_SUCCESS_CAT("LAS", "Billboard AABB created");
    return mesh;
}

// Add AABB from mesh
inline size_t addAABBFromMesh(std::unique_ptr<Mesh> mesh, uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    if (!mesh) return rtx().las_procedural_primitives.size();

    mesh->computeAABB();

    UniversalPrimitive p{};
    p.aabbMin = glm::vec4(mesh->aabbMin, 0.0f);
    p.aabbMax = glm::vec4(mesh->aabbMax, 0.0f);
    p.transform = transform;
    p.type = 0;
    p.materialIndex = materialIndex;

    rtx().las_procedural_primitives.push_back(p);
    rtx().las_procedural_dirty = true;
    rtx().las_tlas_dirty = true;

    return rtx().las_procedural_primitives.size() - 1;
}

// Add procedural AABB
inline size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                                uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    UniversalPrimitive p{};
    p.aabbMin = glm::vec4(center - glm::vec3(scale), 0.0f);
    p.aabbMax = glm::vec4(center + glm::vec3(scale), 0.0f);
    p.transform = transform;
    p.type = static_cast<uint32_t>(type);
    p.materialIndex = materialIndex;

    rtx().las_procedural_primitives.push_back(p);
    rtx().las_procedural_dirty = true;
    rtx().las_tlas_dirty = true;

    return rtx().las_procedural_primitives.size() - 1;
}

// Default hybrid scene
inline void createDefaultHybridScene() noexcept {
    addAABBFromMesh(createPlane(5000.0f, 5000.0f), 0);
    addAABBFromMesh(createBillboard(), 1);

    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(0,5,0), 2.0f, 2);
    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(4,5,4), 1.5f, 3);
    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(-4,5,-4), 1.5f, 4);

    float ringRadius = 10.0f;
    for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(i) * (3.14159f * 2.0f / 6.0f);
        glm::vec3 pos(std::cos(angle) * ringRadius, 3.0f, std::sin(angle) * ringRadius);
        addProceduralAABB(GeometryType::ProceduralD6, pos, 2.0f, static_cast<uint32_t>(5 + i),
                          glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0)));
    }

    addProceduralAABB(GeometryType::ProceduralD100, glm::vec3(0,7,-14), 4.0f, 11,
                      glm::rotate(glm::mat4(1.0f), 0.25f, glm::vec3(0,1,0)));

    addProceduralAABB(GeometryType::ProceduralCylinder, glm::vec3(-15,10,-15), 2.0f, 6);
    addProceduralAABB(GeometryType::ProceduralCone, glm::vec3(0,15,0), 5.0f, 7);

    LOG_SUCCESS_CAT("LAS", "Default hybrid scene created — {} primitives", rtx().las_procedural_primitives.size());
}

// Resize handler
inline void onResize() noexcept {
    rtx().las_tlas_dirty = true;
    rtx().las_procedural_dirty = true;
    LOG_INFO_CAT("LAS", "Resize detected — marked dirty for rebuild");
}

// GLM → Vulkan transform matrix
inline VkTransformMatrixKHR to_vk_transform(const glm::mat4& m) noexcept {
    VkTransformMatrixKHR vkMat{};
    vkMat.matrix[0][0] = m[0][0]; vkMat.matrix[0][1] = m[1][0]; vkMat.matrix[0][2] = m[2][0]; vkMat.matrix[0][3] = m[3][0];
    vkMat.matrix[1][0] = m[0][1]; vkMat.matrix[1][1] = m[1][1]; vkMat.matrix[1][2] = m[2][1]; vkMat.matrix[1][3] = m[3][1];
    vkMat.matrix[2][0] = m[0][2]; vkMat.matrix[2][1] = m[1][2]; vkMat.matrix[2][2] = m[2][2]; vkMat.matrix[2][3] = m[3][2];
    return vkMat;
}

// Ensure LAS is ready (build if dirty)
inline void ensureReady(VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    if (Swapchain::minimized || !Swapchain::swapchain.valid()) {
        LOG_WARNING_CAT("LAS", "Swapchain minimized or invalid — skipping LAS rebuild");
        return;
    }

    if (rtx().las_initialized && !rtx().las_tlas_dirty && !rtx().las_procedural_dirty && rtx().las_tlas != VK_NULL_HANDLE) {
        return;
    }

    if (!rtx().las_initialized) {
        createDefaultHybridScene();
        rtx().las_initialized = true;
    }

    VkCommandBuffer localCmd = cmd;
    bool ownsCmd = (cmd == VK_NULL_HANDLE);

    if (ownsCmd) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = rtx().transient_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(rtx().device, &allocInfo, &localCmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(localCmd, &beginInfo);
    }

    std::vector<std::pair<VkBuffer, VkDeviceMemory>> pendingStaging;

    auto safeUpload = [&](uint64_t handle, const void* data, VkDeviceSize size) {
        auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(handle, data, size, localCmd);
        if (stagingBuf != VK_NULL_HANDLE) pendingStaging.emplace_back(stagingBuf, stagingMem);
    };

    if (rtx().las_procedural_dirty) {
        VkDeviceSize primSize = rtx().las_procedural_primitives.size() * sizeof(UniversalPrimitive);
        if (primSize == 0) primSize = 16;

        if (rtx().las_universal_primitives_buffer == 0) {
            rtx().las_universal_primitives_buffer = Memory::createBuffer(primSize,
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                          "LAS_Primitives");
        }

        safeUpload(rtx().las_universal_primitives_buffer,
                   rtx().las_procedural_primitives.data(),
                   primSize);
        rtx().las_procedural_dirty = false;
    }

    if (rtx().las_tlas_dirty) {
        VkDeviceSize instanceCount = rtx().las_procedural_primitives.size();
        VkDeviceSize instanceSize = instanceCount * sizeof(VkAccelerationStructureInstanceKHR);
        if (instanceSize == 0) instanceSize = 64;

        if (rtx().las_instance_buffer == 0) {
            rtx().las_instance_buffer = Memory::createBuffer(instanceSize,
                                                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                             "LAS_Instances");
        }

        std::vector<VkAccelerationStructureInstanceKHR> instances(instanceCount);
        for (size_t i = 0; i < instanceCount; ++i) {
            const auto& prim = rtx().las_procedural_primitives[i];
            instances[i].transform = to_vk_transform(prim.transform);
            instances[i].instanceCustomIndex = static_cast<uint32_t>(i) & 0xFFFFFFu; // 24-bit limit
            instances[i].mask = 0xFF;
            instances[i].instanceShaderBindingTableRecordOffset = 0;
            instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instances[i].accelerationStructureReference = 0;
        }

        safeUpload(rtx().las_instance_buffer, instances.data(), instanceSize);

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geom.geometry.instances.arrayOfPointers = VK_FALSE;
        geom.geometry.instances.data.deviceAddress = Memory::getDeviceAddress(rtx().las_instance_buffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = static_cast<uint32_t>(instanceCount);
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;  // FIXED: explicit sType

        ext().vkGetAccelerationStructureBuildSizesKHR(rtx().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                      &buildInfo, &range.primitiveCount, &sizes);

        VkDeviceAddress scratchAddr = Memory::allocateScratch(sizes.buildScratchSize);
        buildInfo.scratchData.deviceAddress = scratchAddr;

        if (rtx().las_tlas == VK_NULL_HANDLE) {
            rtx().las_tlas_storage = Memory::createBuffer(sizes.accelerationStructureSize,
                                                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                          "LAS_TLAS");

            auto* storage = Memory::get(rtx().las_tlas_storage);
            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.buffer = storage->buffer;
            createInfo.size = sizes.accelerationStructureSize;

            ext().vkCreateAccelerationStructureKHR(rtx().device, &createInfo, nullptr, &rtx().las_tlas);
        }

        buildInfo.dstAccelerationStructure = rtx().las_tlas;

        ext().vkCmdBuildAccelerationStructuresKHR(localCmd, 1, &buildInfo, &pRange);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        vkCmdPipelineBarrier(localCmd,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);

        rtx().las_tlas_dirty = false;
    }

    if (ownsCmd) {
        vkEndCommandBuffer(localCmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &localCmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(rtx().graphics_queue);

        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &localCmd);

        for (auto& p : pendingStaging) {
            vkDestroyBuffer(rtx().device, p.first, nullptr);
            vkFreeMemory(rtx().device, p.second, nullptr);
        }
    }

    LOG_SUCCESS_CAT("LAS", "LAS update finished");
}

// Get top-level acceleration structure
[[nodiscard]] inline VkAccelerationStructureKHR getTLAS() noexcept {
    ensureReady();
    return rtx().las_tlas;
}