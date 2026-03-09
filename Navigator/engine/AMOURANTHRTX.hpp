#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

// Vulkan 1.4 (2026) — ray tracing is stable, no beta flag needed
#define VK_KHR_acceleration_structure 1
#define VK_KHR_ray_tracing_pipeline 1
#define VK_KHR_deferred_host_operations 1
#define VK_KHR_buffer_device_address 1
#define VK_EXT_descriptor_buffer 1

// Core includes
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Standard library includes
#include <algorithm>
#include <array>
#include <cstdint>
#include <cfloat>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstring>
#include <limits>

// Third-party includes
#include <glm/glm.hpp>

// Required Vulkan device extensions (2026 stable)
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
    appInfo.pApplicationName   = "AMOURANTHRTX";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 9);
    appInfo.pEngineName        = "LIVING WORLD";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 9);
    appInfo.apiVersion         = VK_API_VERSION_1_4;  // 2026 baseline

    uint32_t sdlCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);

    std::vector<const char*> extensions(sdlExts, sdlExts + sdlCount);
    std::vector<const char*> layers;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkInstance inst = VK_NULL_HANDLE;
    vkCreateInstance(&ci, nullptr, &inst);

    return inst;
}

// Geometry type enum (used for procedural BLAS)
enum class GeometryType : uint32_t {
    ProceduralPlane      = 0,
    ProceduralSphere     = 1,
    ProceduralCylinder   = 2,
    ProceduralCone       = 3,
    ProceduralWaterPlane = 4,
};

// Universal primitive (for BLAS procedural geometry)
struct UniversalPrimitive {
    glm::vec4 aabbMin;  // Local AABB min
    glm::vec4 aabbMax;  // Local AABB max
    glm::mat4 transform;  // Local to world transform
    uint32_t  type          = 0;   // GeometryType
    uint32_t  materialIndex = 0;
    float     destruction   = 0.0f;
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

// VRAM reality tracking
struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = 256ULL << 20;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
    VkDeviceSize max_alloc_size   = 0;
    uint32_t     max_alloc_count  = 0;
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

    uint64_t las_aabb_buffer = 0;
    uint64_t las_universal_primitives_buffer = 0;
    VkAccelerationStructureKHR las_as = VK_NULL_HANDLE;
    uint64_t las_as_storage = 0;

    std::vector<UniversalPrimitive> las_procedural_primitives;

    bool las_initialized = false;
    bool las_dirty = true;

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
};

inline VulkanExtensions& ext() noexcept {
    static VulkanExtensions e;
    static bool loaded = false;

    if (loaded) return e;

    VkInstance inst = rtx().instance;
    if (inst == VK_NULL_HANDLE) return e;

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());

    if (!vkGetInstanceProcAddr) return e;

    // Instance functions
    e.vkGetPhysicalDeviceSurfaceSupportKHR      = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    e.vkGetPhysicalDeviceSurfaceFormatsKHR      = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    e.vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    e.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));

    VkDevice dev = rtx().device;
    if (dev == VK_NULL_HANDLE) {
        loaded = true;
        return e;
    }

    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(inst, "vkGetDeviceProcAddr"));

    if (!vkGetDeviceProcAddr) return e;

    // Device functions (same as before)
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

    loaded = true;
    return e;
}

namespace Memory {

inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE      = 256ULL << 20;
inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD  = 1ULL << 20;

enum class MemoryHint : uint8_t { Auto = 0, DeviceLocalOnly = 1, HostVisible = 2, DescriptorBuffer = 3 };

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
    VkCommandBuffer cmd = VK_NULL_HANDLE,
    VkDeviceSize dstOffset = 0
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

    VkMemoryAllocateInfo stageMai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, stageReq.size, stageMemType };

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
    copy.dstOffset = info->offset + dstOffset;
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
}

} // namespace Memory

inline void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout,
                                  VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                  VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) noexcept
{
    if (oldLayout == newLayout || image == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Default access masks — safe fallback (0 is valid for many transitions)
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    // ────────────────────────────────────────────────────────────────
    // Common & safe transitions with precise access + stage masks
    // These cover 99% of real-world usage in your pipeline
    // ────────────────────────────────────────────────────────────────
    switch (oldLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Fresh image (after creation, swapchain acquire, or recreation)
            if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            } else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            // After compute write → ready for copy/blit
            if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                // Rare — only if you ever present HDR directly (not recommended)
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // After blit/copy → ready for present
            if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            } else if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // After copy/blit read → back to compute
            if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            // After present → ready for transfer (swapchain acquire case)
            if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            break;

        default:
            // Unknown/unsupported old layout — log and use generic fallback
            LOG_WARNING_CAT("TRANSITION", "Unsupported old layout: {}", static_cast<int>(oldLayout));
            break;
    }

    // Apply the barrier with the computed (or fallback) stages
    vkCmdPipelineBarrier(cmd,
                         srcStageMask,
                         dstStageMask,
                         0,                  // dependencyFlags
                         0, nullptr,         // memory barriers
                         0, nullptr,         // buffer barriers
                         1, &barrier);
}

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
    vkEnumeratePhysicalDevices(inst, &count, nullptr);

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(inst, &count, devices.data());

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
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);

        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());

        bool has_all = true;
        for (const char* req : requiredDeviceExtensions) {
            bool found = false;
            for (const auto& avail : exts) {
                if (std::strcmp(avail.extensionName, req) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                has_all = false;
                break;
            }
        }
        if (!has_all) continue;

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

    rtx().physical = selected;

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(selected, &features);

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.shaderFloat64 = VK_TRUE;

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

    VkDeviceCreateInfo dev_ci{};
    dev_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_ci.pNext = &desc_buf;
    dev_ci.pEnabledFeatures = &enabledFeatures;
    dev_ci.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    dev_ci.pQueueCreateInfos = queue_infos.data();
    dev_ci.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    dev_ci.ppEnabledExtensionNames = enabledExtensions.data();

    VkDevice dev = VK_NULL_HANDLE;
    vkCreateDevice(selected, &dev_ci, nullptr, &dev);

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

    Memory::init();

    return dev;
}

struct Swapchain {
    struct Handle {
        VkSwapchainKHR value;
        Handle() = default;
        explicit Handle(VkSwapchainKHR v) : value(v) {}
        VkSwapchainKHR get() const noexcept { return value; }
        void reset() noexcept { value = VK_NULL_HANDLE; }
        bool valid() const noexcept { return value != VK_NULL_HANDLE; }
        operator VkSwapchainKHR() const noexcept { return value; }
    };

    inline static Handle                swapchain;
    inline static std::vector<VkImage>  images;
    inline static std::vector<VkImageView> views;
    inline static VkExtent2D            extent          {};
    inline static VkFormat              format          = VK_FORMAT_UNDEFINED;
    inline static VkColorSpaceKHR       colorSpace      = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    inline static VkPresentModeKHR      presentMode     = VK_PRESENT_MODE_FIFO_KHR;
    inline static bool                  minimized       = false;
    inline static bool                  supportsStorage = false;

    // Timing / refresh estimation (used by RayCanvas for pacing)
    inline static double lastPresentTime_s   = 0.0;
    inline static double smoothedRefresh_s   = 1.0 / 60.0;

    // ────────────────────────────────────────────────────────────────
    // Create or recreate swapchain
    // ────────────────────────────────────────────────────────────────
    static void createOrRecreate(int requestedWidth, int requestedHeight, bool isRecreate = false) noexcept {
        VkDevice device = rtx().device;
        if (!device) return;

        vkDeviceWaitIdle(device);
        vkQueueWaitIdle(rtx().present_queue);

        if (isRecreate) {
            cleanupViewsAndSwapchain();
            minimized = false;
        }

        VkSurfaceCapabilitiesKHR caps{};
        ext().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rtx().physical, rtx().surface, &caps);

        // Handle minimized / invalid surface
        if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
            minimized = true;
            LOG_WARNING_CAT("SWAPCHAIN", "Surface reports 0×0 extent — window likely minimized");
            return;
        }

        // Choose extent
        VkExtent2D newExtent = caps.currentExtent;
        if (newExtent.width == std::numeric_limits<uint32_t>::max()) {
            newExtent.width  = std::clamp(static_cast<uint32_t>(requestedWidth),
                                          caps.minImageExtent.width,
                                          caps.maxImageExtent.width);
            newExtent.height = std::clamp(static_cast<uint32_t>(requestedHeight),
                                          caps.minImageExtent.height,
                                          caps.maxImageExtent.height);
        }

        if (newExtent.width == 0 || newExtent.height == 0) {
            minimized = true;
            return;
        }

        extent = newExtent;

        // ── Surface formats ─────────────────────────────────────────────
        uint32_t fmtCount = 0;
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, formats.data());

        VkSurfaceFormatKHR chosenFmt = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];

        constexpr std::array preferred = {
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB,  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB,  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
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

        format     = chosenFmt.format;
        colorSpace = chosenFmt.colorSpace;

        // ── Present modes ───────────────────────────────────────────────
        uint32_t pmCount = 0;
        ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, nullptr);
        std::vector<VkPresentModeKHR> modes(pmCount);
        ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, modes.data());

        VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;  // safe fallback

        if (!Options::Window::VSYNC) {
            // Our own relaxed pacing is in control → prefer IMMEDIATE for lowest latency
            // Timing cache prevents over-submit / excessive tearing
            if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
                chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
            // No MAILBOX attempt — we know it's unavailable and not needed
        } 
        // else: user explicitly wants driver V-Sync → FIFO (zero tearing, higher latency)

        presentMode = chosenPM;

        // ── Image count ─────────────────────────────────────────────────
        uint32_t imgCount = std::max(caps.minImageCount, 2u);
        if (caps.maxImageCount > 0) {
            imgCount = std::min(imgCount, caps.maxImageCount);
        }

        // ── Image usage ─────────────────────────────────────────────────
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        supportsStorage = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0;

        // ── Sharing mode ────────────────────────────────────────────────
        uint32_t queueFamilyIndices[2] = {rtx().graphics_family, rtx().present_family};

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = rtx().surface;
        ci.minImageCount    = imgCount;
        ci.imageFormat      = format;
        ci.imageColorSpace  = colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = usage;
        ci.preTransform     = caps.currentTransform;
        ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode      = presentMode;
        ci.clipped          = VK_TRUE;
        ci.oldSwapchain     = isRecreate ? swapchain.get() : VK_NULL_HANDLE;

        if (rtx().graphics_family != rtx().present_family) {
            ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            ci.queueFamilyIndexCount = 0;
            ci.pQueueFamilyIndices   = nullptr;
        }

        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        VkResult res = ext().vkCreateSwapchainKHR(device, &ci, nullptr, &newSwap);
        if (res != VK_SUCCESS) {
            minimized = true;
            return;
        }

        swapchain = Handle(newSwap);

        // ── Get images ──────────────────────────────────────────────────
        uint32_t actualCount = 0;
        ext().vkGetSwapchainImagesKHR(device, newSwap, &actualCount, nullptr);
        images.resize(actualCount);
        ext().vkGetSwapchainImagesKHR(device, newSwap, &actualCount, images.data());

        rtx().image_count = actualCount;

        // ── Create views ────────────────────────────────────────────────
        views.resize(actualCount);
        for (uint32_t i = 0; i < actualCount; ++i) {
            VkImageViewCreateInfo vi{};
            vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vi.image            = images[i];
            vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
            vi.format           = format;
            vi.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            vkCreateImageView(device, &vi, nullptr, &views[i]);
        }

        lastPresentTime_s = 0.0;
        smoothedRefresh_s = 1.0 / 60.0;

        LOG_SUCCESS_CAT("SWAPCHAIN", "Created swapchain {}x{} ({} images, present mode {})",
                        extent.width, extent.height, actualCount, static_cast<int>(presentMode));
    }

    // ────────────────────────────────────────────────────────────────
    // Blit from internal HDR render target → swapchain image (with scale)
    // ────────────────────────────────────────────────────────────────
    static void scaleBlit(VkCommandBuffer cmd,
                          VkImage srcImage,
                          VkExtent2D srcExtent,
                          VkImage dstImage,
                          VkExtent2D dstExtent) noexcept {
        if (!cmd || srcImage == VK_NULL_HANDLE || dstImage == VK_NULL_HANDLE) return;

        // Assume srcImage is already in TRANSFER_SRC_OPTIMAL (set by caller)
        // Transition dst to TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = dstImage;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit region{};
        region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.srcOffsets[0] = { 0, 0, 0 };
        region.srcOffsets[1] = { static_cast<int32_t>(srcExtent.width),
                                static_cast<int32_t>(srcExtent.height), 1 };

        region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.dstOffsets[0] = { 0, 0, 0 };
        region.dstOffsets[1] = { static_cast<int32_t>(dstExtent.width),
                                static_cast<int32_t>(dstExtent.height), 1 };

        vkCmdBlitImage(cmd,
                       srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region,
                       VK_FILTER_LINEAR);

        // Transition dst to PRESENT_SRC_KHR
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // ────────────────────────────────────────────────────────────────
    // Helpers
    // ────────────────────────────────────────────────────────────────
    static void cleanupViewsAndSwapchain() noexcept {
        VkDevice dev = rtx().device;
        if (!dev) return;

        for (auto view : views) {
            if (view) vkDestroyImageView(dev, view, nullptr);
        }
        views.clear();
        images.clear();

        if (swapchain.valid()) {
            ext().vkDestroySwapchainKHR(dev, swapchain.get(), nullptr);
            swapchain.reset();
        }
    }

    static void cleanup() noexcept {
        cleanupViewsAndSwapchain();
        lastPresentTime_s = 0.0;
        smoothedRefresh_s = 1.0 / 60.0;
        minimized = false;
    }

    static void create(SDL_Window* window, int w, int h) noexcept {
        rtx().window = window;
        createOrRecreate(w, h, false);
    }

    static void recreate(int w, int h) noexcept {
        createOrRecreate(w, h, true);
    }

    static bool isMinimized() noexcept { return minimized; }
    static VkSwapchainKHR get() noexcept { return swapchain.get(); }
    static VkExtent2D getExtent() noexcept { return extent; }
    static VkFormat getFormat() noexcept { return format; }
    static bool canDirectWrite() noexcept { return supportsStorage; }
    static double getSmoothedRefresh() noexcept { return smoothedRefresh_s; }

    static void updateRefreshEstimate(double presentTime_s) noexcept {
        if (lastPresentTime_s > 0.0) {
            double delta = presentTime_s - lastPresentTime_s;
            if (delta > 0.0005 && delta < 0.2) {
                constexpr double alpha = 0.25;
                smoothedRefresh_s = alpha * delta + (1.0 - alpha) * smoothedRefresh_s;
            }
        }
        lastPresentTime_s = presentTime_s;
    }
};

// Resize handler
inline void onResize() noexcept {
    rtx().las_dirty = true;
}

// GLM → Vulkan transform matrix
inline VkTransformMatrixKHR to_vk_transform(const glm::mat4& m) noexcept {
    VkTransformMatrixKHR vkMat{};
    vkMat.matrix[0][0] = m[0][0]; vkMat.matrix[0][1] = m[1][0]; vkMat.matrix[0][2] = m[2][0]; vkMat.matrix[0][3] = m[3][0];
    vkMat.matrix[1][0] = m[0][1]; vkMat.matrix[1][1] = m[1][1]; vkMat.matrix[1][2] = m[2][1]; vkMat.matrix[1][3] = m[3][1];
    vkMat.matrix[2][0] = m[0][2]; vkMat.matrix[2][1] = m[1][2]; vkMat.matrix[2][2] = m[2][2]; vkMat.matrix[2][3] = m[3][2];
    return vkMat;
}

// Ensure LAS is ready (build if dirty) — using correct Vulkan 1.4 struct: VkAabbPositionsKHR
inline void ensureReady(VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    if (Swapchain::minimized || !Swapchain::swapchain.valid()) {
        return;
    }

    if (rtx().las_initialized && !rtx().las_dirty && rtx().las_as != VK_NULL_HANDLE) {
        return;
    }

    if (!rtx().las_initialized) {
        rtx().las_initialized = true;
    }

    VkDevice dev = rtx().device;
    VkCommandBuffer localCmd = cmd;
    bool ownsCmd = (cmd == VK_NULL_HANDLE);

    if (ownsCmd) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = rtx().transient_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(dev, &allocInfo, &localCmd);

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

    VkDeviceSize primCount = rtx().las_procedural_primitives.size();
    VkDeviceSize primSize = primCount * sizeof(UniversalPrimitive);
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

    // Compute world AABBs — using correct struct VkAabbPositionsKHR
    std::vector<VkAabbPositionsKHR> aabbs(primCount);
    for (size_t i = 0; i < primCount; ++i) {
        const auto& prim = rtx().las_procedural_primitives[i];
        glm::vec3 local_min = glm::vec3(prim.aabbMin);
        glm::vec3 local_max = glm::vec3(prim.aabbMax);

        glm::vec3 corners[8] = {
            {local_min.x, local_min.y, local_min.z},
            {local_min.x, local_min.y, local_max.z},
            {local_min.x, local_max.y, local_min.z},
            {local_min.x, local_max.y, local_max.z},
            {local_max.x, local_min.y, local_min.z},
            {local_max.x, local_min.y, local_max.z},
            {local_max.x, local_max.y, local_min.z},
            {local_max.x, local_max.y, local_max.z}
        };

        glm::vec3 world_min(std::numeric_limits<float>::max());
        glm::vec3 world_max(std::numeric_limits<float>::lowest());

        for (const auto& corner : corners) {
            glm::vec4 transformed = prim.transform * glm::vec4(corner, 1.0f);
            glm::vec3 pos = glm::vec3(transformed) / transformed.w;
            world_min = glm::min(world_min, pos);
            world_max = glm::max(world_max, pos);
        }

        aabbs[i].minX = world_min.x;
        aabbs[i].minY = world_min.y;
        aabbs[i].minZ = world_min.z;
        aabbs[i].maxX = world_max.x;
        aabbs[i].maxY = world_max.y;
        aabbs[i].maxZ = world_max.z;
    }

    VkDeviceSize aabbSize = primCount * sizeof(VkAabbPositionsKHR);
    if (aabbSize == 0) aabbSize = 16;

    if (rtx().las_aabb_buffer == 0) {
        rtx().las_aabb_buffer = Memory::createBuffer(aabbSize,
                                                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      "LAS_AABBs");
    }

    safeUpload(rtx().las_aabb_buffer, aabbs.data(), aabbSize);

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    geom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    geom.geometry.aabbs.data.deviceAddress = Memory::getDeviceAddress(rtx().las_aabb_buffer);
    geom.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);  // ← FIXED: correct struct name

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(primCount);
    range.primitiveOffset = 0;
    range.firstVertex = 0;
    range.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    ext().vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                  &buildInfo, &range.primitiveCount, &sizes);

    VkDeviceAddress scratchAddr = Memory::allocateScratch(sizes.buildScratchSize);
    buildInfo.scratchData.deviceAddress = scratchAddr;

    if (rtx().las_as == VK_NULL_HANDLE) {
        rtx().las_as_storage = Memory::createBuffer(sizes.accelerationStructureSize,
                                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                    "LAS_AS");

        auto* storage = Memory::get(rtx().las_as_storage);
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        createInfo.buffer = storage->buffer;
        createInfo.size = sizes.accelerationStructureSize;

        ext().vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rtx().las_as);
    }

    buildInfo.dstAccelerationStructure = rtx().las_as;

    ext().vkCmdBuildAccelerationStructuresKHR(localCmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(localCmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    rtx().las_dirty = false;

    if (ownsCmd) {
        vkEndCommandBuffer(localCmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &localCmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(rtx().graphics_queue);

        vkFreeCommandBuffers(dev, rtx().transient_pool, 1, &localCmd);

        for (auto& p : pendingStaging) {
            vkDestroyBuffer(dev, p.first, nullptr);
            vkFreeMemory(dev, p.second, nullptr);
        }
    }
}

// Get acceleration structure
[[nodiscard]] inline VkAccelerationStructureKHR getAS() noexcept {
    ensureReady();
    return rtx().las_as;
}

// Additional: Init function for the engine
inline bool initRTX(SDL_Window* window, int width, int height) noexcept {
    rtx().instance = createVulkanInstance();
    if (rtx().instance == VK_NULL_HANDLE) return false;

    if (!SDL_Vulkan_CreateSurface(window, rtx().instance, nullptr, &rtx().surface)) return false;

    rtx().device = createLogicalDeviceAndSelectGPU(rtx().instance, rtx().surface);
    if (rtx().device == VK_NULL_HANDLE) return false;

    ext();  // Load extensions

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;
    vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);

    Swapchain::create(window, width, height);

    return true;
}

// Cleanup function
inline void cleanupRTX() noexcept {
    if (rtx().transient_pool != VK_NULL_HANDLE) vkDestroyCommandPool(rtx().device, rtx().transient_pool, nullptr);
    Swapchain::cleanup();
    if (rtx().device != VK_NULL_HANDLE) vkDestroyDevice(rtx().device, nullptr);
    if (rtx().surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(rtx().instance, rtx().surface, nullptr);
    if (rtx().instance != VK_NULL_HANDLE) vkDestroyInstance(rtx().instance, nullptr);
}