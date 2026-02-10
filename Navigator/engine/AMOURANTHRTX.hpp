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
#include <array>
#include <cstdint>
#include <utility>
#include <vector>
#include <future>
#include <unordered_map>
#include <mutex>
#include <string>
#include <source_location>
#include <ctime>
#include <set>
#include <format>
#include <stacktrace>
#include <map>

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

// Required Vulkan device extensions
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

// Memory constants
inline constexpr VkDeviceSize TINY_SAFETY_MARGIN = 256ULL << 20;

// Geometry type enum — defensive, explicit
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

// Acceleration structure structs — pure procedural AABB only
struct UniversalPrimitive {
    glm::vec4 aabbMin;
    glm::vec4 aabbMax;
    glm::mat4 transform;
    uint32_t type          = 0;
    uint32_t materialIndex = 0;
    float destruction      = 0.0f;
};

// Buffer management structs
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

struct Chunk {
    VkBuffer         buffer   = VK_NULL_HANDLE;
    VkDeviceMemory   memory   = VK_NULL_HANDLE;
    VkDeviceSize     size     = 0;
    VkDeviceAddress  baseAddr = 0;
    VkDeviceSize     head     = 0;
    std::string      tag;
    std::vector<uint64_t> handles;
};

// VRAM reality struct
struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = TINY_SAFETY_MARGIN;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
    VkDeviceSize max_alloc_size   = 0;
    uint32_t max_alloc_count      = 0;
};

// Global RTX context singleton — pure procedural only
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

    std::vector<Chunk> buffer_chunks;
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
    // Surface/instance extensions
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;

    // Swapchain
    PFN_vkCreateSwapchainKHR                      vkCreateSwapchainKHR                      = nullptr;
    PFN_vkDestroySwapchainKHR                     vkDestroySwapchainKHR                     = nullptr;
    PFN_vkGetSwapchainImagesKHR                   vkGetSwapchainImagesKHR                   = nullptr;
    PFN_vkAcquireNextImageKHR                     vkAcquireNextImageKHR                     = nullptr;
    PFN_vkQueuePresentKHR                         vkQueuePresentKHR                         = nullptr;

    // Ray tracing pipeline
    PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR            = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR      = nullptr;
    PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR                         = nullptr;

    // Acceleration structures
    PFN_vkGetAccelerationStructureBuildSizesKHR   vkGetAccelerationStructureBuildSizesKHR   = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR       = nullptr;
    PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR          = nullptr;
    PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR         = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // Buffer device address
    PFN_vkGetBufferDeviceAddress                  vkGetBufferDeviceAddress                  = nullptr;

    // Acceleration structure utilities
    PFN_vkCmdCopyAccelerationStructureKHR         vkCmdCopyAccelerationStructureKHR         = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCmdTraceRaysIndirect2KHR                vkCmdTraceRaysIndirect2KHR                = nullptr;

    // Dynamic rendering
    PFN_vkCmdBeginRendering                       vkCmdBeginRendering                       = nullptr;
    PFN_vkCmdEndRendering                         vkCmdEndRendering                         = nullptr;
    PFN_vkCmdPipelineBarrier2                     vkCmdPipelineBarrier2                     = nullptr;
    PFN_vkQueueSubmit2KHR                         vkQueueSubmit2KHR                         = nullptr;

    // Debug utils
    PFN_vkSetDebugUtilsObjectNameEXT              vkSetDebugUtilsObjectNameEXT              = nullptr;

    // Descriptor buffer
    PFN_vkGetDescriptorSetLayoutSizeEXT           vkGetDescriptorSetLayoutSizeEXT           = nullptr;
    PFN_vkGetDescriptorSetLayoutBindingOffsetEXT  vkGetDescriptorSetLayoutBindingOffsetEXT  = nullptr;
    PFN_vkCmdBindDescriptorBuffersEXT             vkCmdBindDescriptorBuffersEXT             = nullptr;
    PFN_vkCmdSetDescriptorBufferOffsetsEXT        vkCmdSetDescriptorBufferOffsetsEXT        = nullptr;
    PFN_vkGetDescriptorEXT                        vkGetDescriptorEXT                        = nullptr;
};

inline VulkanExtensions& ext() noexcept {
    static VulkanExtensions e;

    static bool instanceLoaded = false;
    static bool deviceLoaded = false;

    if (!instanceLoaded) {
        VkInstance inst = rtx().instance;
        if (inst) {
            PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                SDL_Vulkan_GetVkGetInstanceProcAddr());

            if (vkGetInstanceProcAddr) {
                e.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
                    vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceSupportKHR"));
                e.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
                    vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
                e.vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
                    vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
                e.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
                    vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));

                instanceLoaded = true;
            }
        }
    }

    if (!deviceLoaded) {
        VkDevice dev = rtx().device;
        if (dev != VK_NULL_HANDLE) {
            PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                SDL_Vulkan_GetVkGetInstanceProcAddr());

            if (vkGetInstanceProcAddr) {
                PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    vkGetInstanceProcAddr(rtx().instance, "vkGetDeviceProcAddr"));

                if (vkGetDeviceProcAddr) {
                    // Swapchain
                    e.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
                        vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
                    e.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
                        vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
                    e.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
                        vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
                    e.vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
                        vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
                    e.vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
                        vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));

                    // Ray tracing pipeline
                    e.vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
                        vkGetDeviceProcAddr(dev, "vkCreateRayTracingPipelinesKHR"));
                    e.vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
                        vkGetDeviceProcAddr(dev, "vkGetRayTracingShaderGroupHandlesKHR"));
                    e.vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
                        vkGetDeviceProcAddr(dev, "vkCmdTraceRaysKHR"));

                    // Acceleration structures — all needed
                    e.vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                        vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureBuildSizesKHR"));
                    e.vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                        vkGetDeviceProcAddr(dev, "vkCmdBuildAccelerationStructuresKHR"));
                    e.vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                        vkGetDeviceProcAddr(dev, "vkCreateAccelerationStructureKHR"));
                    e.vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                        vkGetDeviceProcAddr(dev, "vkDestroyAccelerationStructureKHR"));
                    e.vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                        vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureDeviceAddressKHR"));

                    // Buffer device address
                    e.vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
                        vkGetDeviceProcAddr(dev, "vkGetBufferDeviceAddress"));

                    // Acceleration structure utilities
                    e.vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
                        vkGetDeviceProcAddr(dev, "vkCmdCopyAccelerationStructureKHR"));
                    e.vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
                        vkGetDeviceProcAddr(dev, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
                    e.vkCmdTraceRaysIndirect2KHR = reinterpret_cast<PFN_vkCmdTraceRaysIndirect2KHR>(
                        vkGetDeviceProcAddr(dev, "vkCmdTraceRaysIndirect2KHR"));

                    // Dynamic rendering
                    e.vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
                        vkGetDeviceProcAddr(dev, "vkCmdBeginRendering"));
                    e.vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
                        vkGetDeviceProcAddr(dev, "vkCmdEndRendering"));
                    e.vkCmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
                        vkGetDeviceProcAddr(dev, "vkCmdPipelineBarrier2"));
                    e.vkQueueSubmit2KHR = reinterpret_cast<PFN_vkQueueSubmit2KHR>(
                        vkGetDeviceProcAddr(dev, "vkQueueSubmit2KHR"));

                    // Debug utils
                    e.vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                        vkGetDeviceProcAddr(dev, "vkSetDebugUtilsObjectNameEXT"));

                    // Descriptor buffer
                    e.vkGetDescriptorSetLayoutSizeEXT = reinterpret_cast<PFN_vkGetDescriptorSetLayoutSizeEXT>(
                        vkGetDeviceProcAddr(dev, "vkGetDescriptorSetLayoutSizeEXT"));
                    e.vkGetDescriptorSetLayoutBindingOffsetEXT = reinterpret_cast<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>(
                        vkGetDeviceProcAddr(dev, "vkGetDescriptorSetLayoutBindingOffsetEXT"));
                    e.vkCmdBindDescriptorBuffersEXT = reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(
                        vkGetDeviceProcAddr(dev, "vkCmdBindDescriptorBuffersEXT"));
                    e.vkCmdSetDescriptorBufferOffsetsEXT = reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(
                        vkGetDeviceProcAddr(dev, "vkCmdSetDescriptorBufferOffsetsEXT"));
                    e.vkGetDescriptorEXT = reinterpret_cast<PFN_vkGetDescriptorEXT>(
                        vkGetDeviceProcAddr(dev, "vkGetDescriptorEXT"));

                    deviceLoaded = true;
                    LOG_SUCCESS_CAT("EXT", "Device extensions loaded successfully");
                } else {
                    LOG_ERROR_CAT("EXT", "Failed to get vkGetDeviceProcAddr");
                }
            }
        } else {
            LOG_WARNING_CAT("EXT", "Device not created yet — skipping device extension load");
        }
    }

    return e;
}

// Memory management namespace — compartmentalized
namespace Memory {

inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE = 256ULL << 20;

inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL << 10;
inline constexpr VkDeviceSize SBT_MINIMUM_SIZE = 512;
inline constexpr VkDeviceSize SBT_ALIGNMENT = 256;

inline constexpr VkBufferUsageFlags CHUNK_USAGE_FLAGS =
    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

inline constexpr VkBufferUsageFlags VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_SCRATCH_BIT_KHR = 0x00080000;

[[nodiscard]] constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept {
    VkPhysicalDevice phys = rtx().physical;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("MEMORY", "No suitable memory type found for filter 0x{}, properties 0x{}", typeFilter, properties);
    return ~0u;
}

[[nodiscard]] inline VRAMReality measureReality() noexcept {
    static bool measured = false;
    static VRAMReality reality{};

    double now_us = TotalTime::get().us();
    static double last_measurement_us = 0.0;

    if (measured) {
        double age_ms = (now_us - last_measurement_us) / 1000.0;
        LOG_INFO_CAT("Memory", "VRAM measurement last performed {} ms ago", static_cast<uint64_t>(age_ms));
        LOG_INFO_CAT("Memory", "Total device-local VRAM: {} bytes ({} GB)", reality.total, reality.total / (1024ULL * 1024 * 1024));
        LOG_INFO_CAT("Memory", "Usable VRAM: {} bytes ({} GB)", reality.usable, reality.usable / (1024ULL * 1024 * 1024));
        LOG_INFO_CAT("Memory", "Remaining VRAM: {} bytes ({} GB)", reality.remaining, reality.remaining / (1024ULL * 1024 * 1024));
        return reality;
    }

    LOG_INFO_CAT("Memory", "Starting VRAM measurement at {} µs since genesis", static_cast<uint64_t>(now_us));

    VkPhysicalDevice phys = rtx().physical;

    if (phys == VK_NULL_HANDLE) {
        vkh.checker(false, "Physical device existence",
                    "No physical device available — measurement aborted");
        return VRAMReality{};
    }

    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(phys, &deviceProps);

    uint32_t api_major = VK_VERSION_MAJOR(deviceProps.apiVersion);
    uint32_t api_minor = VK_VERSION_MINOR(deviceProps.apiVersion);
    uint32_t api_patch = VK_VERSION_PATCH(deviceProps.apiVersion);

    uint32_t driver_major = VK_VERSION_MAJOR(deviceProps.driverVersion);
    uint32_t driver_minor = VK_VERSION_MINOR(deviceProps.driverVersion);
    uint32_t driver_patch = VK_VERSION_PATCH(deviceProps.driverVersion);

    LOG_INFO_CAT("Memory", "Selected physical device: {}", deviceProps.deviceName);
    LOG_INFO_CAT("Memory", "Device ID: {}", deviceProps.deviceID);
    LOG_INFO_CAT("Memory", "Vendor ID: 0x{}", deviceProps.vendorID);
    LOG_INFO_CAT("Memory", "Device type: {}", 
                 deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete GPU" :
                 deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated GPU" :
                 deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "virtual GPU" :
                 deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU device" : "unknown");
    LOG_INFO_CAT("Memory", "Vulkan API version: {}.{}.{}", api_major, api_minor, api_patch);
    LOG_INFO_CAT("Memory", "Driver version: {}.{}.{}", driver_major, driver_minor, driver_patch);

    VkPhysicalDeviceMemoryProperties2 memProps2{};
    memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;

    vkGetPhysicalDeviceMemoryProperties2(phys, &memProps2);
    const VkPhysicalDeviceMemoryProperties& mem = memProps2.memoryProperties;

    std::string type_line = "Memory types:\n";
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        const VkMemoryType& t = mem.memoryTypes[i];
        std::string typeProps;
        if (t.propertyFlags == 0) {
            typeProps = "none";
        } else {
            if (t.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) typeProps += "device-local ";
            if (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) typeProps += "host-visible ";
            if (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) typeProps += "host-coherent ";
            if (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) typeProps += "host-cached ";
            if (t.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) typeProps += "lazily-allocated ";
            if (t.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) typeProps += "protected ";
            if (!typeProps.empty()) typeProps.pop_back();
        }
        type_line += std::format("Type {} (heap {}): [{}]; \n", i, t.heapIndex, typeProps);
    }
    LOG_INFO_CAT("Memory", "\n{}", type_line);

    std::string heap_line = "Memory heaps:\n";
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        const VkMemoryHeap& h = mem.memoryHeaps[i];
        std::string flags = (h.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "device-local" : "non-device-local";
        if (h.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) flags += ", multi-instance";
        heap_line += std::format("Heap {}: {} bytes ({} GB) [{}]; \n", 
                                 i, h.size, h.size / (1024ULL * 1024 * 1024), flags);
    }
    LOG_INFO_CAT("Memory", "\n{}", heap_line);

    reality.total = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.total += mem.memoryHeaps[i].size;
        }
    }
    LOG_INFO_CAT("Memory", "Total device-local VRAM capacity: {} bytes ({} GB)", 
                 reality.total, reality.total / (1024ULL * 1024 * 1024));

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    memProps2.pNext = &budget;

    vkGetPhysicalDeviceMemoryProperties2(phys, &memProps2);

    std::string budget_line = "\nDriver budget & usage: \n";
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budget.heapBudget[i] || budget.heapUsage[i]) {
            budget_line += std::format("Heap {}: budget {} bytes ({} GB), usage {} bytes ({} GB); \n",
                                       i,
                                       budget.heapBudget[i], budget.heapBudget[i] / (1024ULL * 1024 * 1024),
                                       budget.heapUsage[i], budget.heapUsage[i] / (1024ULL * 1024 * 1024));
        }
    }
    LOG_INFO_CAT("Memory", "{}", budget_line.empty() ? "Driver provided no budget or usage data" : budget_line);

    reality.driver_footprint = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.driver_footprint = budget.heapUsage[i];
            LOG_INFO_CAT("Memory", "Current driver/OS usage on device-local VRAM: {} bytes and budget limit {} GB)",
                         reality.driver_footprint,
                         budget.heapBudget[i] / (1024ULL * 1024 * 1024));
            break;
        }
    }

    if (reality.driver_footprint == 0) {
        LOG_WARNING_CAT("Memory", "Driver reported zero usage — using conservative 1,500,000,000 byte estimate");
        reality.driver_footprint = 1'500'000'000ULL;
    }

    reality.usable = (reality.total > (reality.driver_footprint + reality.safety_margin))
                   ? reality.total - reality.driver_footprint - reality.safety_margin
                   : 0;
    reality.remaining = reality.usable;

    reality.max_alloc_count = deviceProps.limits.maxMemoryAllocationCount;

    reality.max_alloc_size = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.max_alloc_size = mem.memoryHeaps[i].size;
            break;
        }
    }

    LOG_INFO_CAT("Memory", "Maximum single allocation size from device-local VRAM: {} bytes ({} GB)",
                 reality.max_alloc_size, reality.max_alloc_size / (1024ULL * 1024 * 1024));
    LOG_INFO_CAT("Memory", "Maximum number of simultaneous allocations allowed: {}", reality.max_alloc_count);

    LOG_SUCCESS_CAT("Memory", "VRAM measurement complete");

    last_measurement_us = now_us;
    measured = true;
    return reality;
}

[[nodiscard]] inline VkDeviceSize availableToTake() noexcept {
    return rtx().vram_reality.remaining;
}

[[nodiscard]] inline Chunk* createChunk(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE) noexcept {
    VkDevice dev = rtx().device;

    if (dev == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("MEMORY", "Cannot create chunk — device is VK_NULL_HANDLE");
        return nullptr;
    }

    VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, size);

    LOG_INFO_CAT("MEMORY", "Creating chunk — size={} bytes, usage=0x{}, sharing={}",
                 chunkSize, usage, sharingMode == VK_SHARING_MODE_EXCLUSIVE ? "exclusive" : "concurrent");

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = chunkSize;
    bci.usage = CHUNK_USAGE_FLAGS | usage;
    bci.sharingMode = sharingMode;

    VkBuffer buffer = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateBuffer(dev, &bci, nullptr, &buffer),
        "vkCreateBuffer (chunk)",
        "Failed to create chunk buffer"
    );

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkh.checker(memType != ~0u, "findMemoryType (chunk)",
                "No device-local memory type for chunk");

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    vkh.checker(
        vkAllocateMemory(dev, &mai, nullptr, &memory),
        "vkAllocateMemory (chunk)",
        "Failed to allocate chunk memory"
    );

    vkh.checker(
        vkBindBufferMemory(dev, buffer, memory, 0),
        "vkBindBufferMemory (chunk)",
        "Failed to bind chunk memory"
    );

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress baseAddr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    std::vector<Chunk>& chunks = rtx().buffer_chunks;
    std::string chunkTag = "Chunk_" + std::to_string(chunks.size());
    chunks.push_back({buffer, memory, chunkSize, baseAddr, 0, chunkTag, {}});

    LOG_SUCCESS_CAT("MEMORY", "Chunk created — tag='{}', size={} bytes, base addr=0x{}", chunkTag, chunkSize, baseAddr);

    return &chunks.back();
}

[[nodiscard]] inline uint64_t createDescriptorBuffer(VkDeviceSize size, std::string_view tag = "EternalDescriptorBuffer") noexcept {
    if (size == 0) size = 4096ULL;

    VkDevice dev = rtx().device;

    if (dev == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("MEMORY", "Cannot create descriptor buffer — device is VK_NULL_HANDLE");
        return 0;
    }

    LOG_INFO_CAT("MEMORY", "Creating descriptor buffer — size={} bytes, tag='{}'", size, tag);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateBuffer(dev, &bci, nullptr, &buffer),
        "vkCreateBuffer (descriptor)",
        "Failed to create descriptor buffer"
    );

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkh.checker(memType != ~0u, "findMemoryType (descriptor)",
                "No host-visible coherent memory for descriptor buffer");

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    vkh.checker(
        vkAllocateMemory(dev, &mai, nullptr, &memory),
        "vkAllocateMemory (descriptor)",
        "Failed to allocate descriptor memory"
    );

    vkh.checker(
        vkBindBufferMemory(dev, buffer, memory, 0),
        "vkBindBufferMemory (descriptor)",
        "Failed to bind descriptor memory"
    );

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    std::unordered_map<uint64_t, BufferInfo>& buffers = rtx().buffers;
    uint64_t handle = ++rtx().next_buffer_handle;
    buffers.emplace(handle, BufferInfo{
        buffer, memory, size, req.alignment, 0, addr,
        nullptr, usage, std::string(tag)
    });

    LOG_SUCCESS_CAT("MEMORY", "Descriptor buffer created — handle={}, size={} bytes, addr=0x{}", handle, size, addr);

    return handle;
}

[[nodiscard]] inline void* lazyMapDescriptor(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    std::unordered_map<uint64_t, BufferInfo>& buffers = rtx().buffers;
    std::unordered_map<uint64_t, BufferInfo>::iterator it = buffers.find(handle);

    if (it == buffers.end()) {
        std::string msg = std::format("lazyMapDescriptor failed — invalid handle {}", handle);
        vkh.checker(false, "lazyMapDescriptor handle lookup", msg.c_str());
        return nullptr;
    }

    BufferInfo& info = it->second;

    bool isDescriptor = (info.usage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) != 0;
    if (!isDescriptor) {
        std::string msg = std::format("lazyMapDescriptor failed — not a descriptor buffer (handle={:016x})", handle);
        vkh.checker(false, "Descriptor buffer usage check", msg.c_str());
        return nullptr;
    }

    if (info.mapped != nullptr) return info.mapped;

    VkDevice dev = rtx().device;
    if (dev == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("MEMORY", "Cannot map descriptor — device is VK_NULL_HANDLE");
        return nullptr;
    }

    void* mapped = nullptr;
    VkResult res = vkMapMemory(dev, info.memory, 0, info.size, 0, &mapped);

    if (res != VK_SUCCESS) {
        std::string msg = std::format("vkMapMemory(descriptor) failed for handle {:016x}", handle);
        vkh.checker(res, "vkMapMemory (descriptor buffer)", msg.c_str());
    }

    info.mapped = mapped;
    LOG_INFO_CAT("MEMORY", "Descriptor buffer mapped — handle={:016x}, mapped ptr=0x{:x}",
                 handle, reinterpret_cast<uintptr_t>(mapped));

    return mapped;
}

[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage,
                                     std::string_view tag = "") noexcept {
    if (size == 0) {
        std::string msg = std::format("create called with size=0 (tag={}) — returning invalid handle", tag);
        vkh.checker(false, "Buffer size validation", msg.c_str());
        return 0;
    }

    LOG_INFO_CAT("BUFFER", "Creating buffer: size={} bytes, usage=0x{:x}, tag='{}'", size, usage, tag);

    VkDevice dev = rtx().device;
    if (dev == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("MEMORY", "Cannot create buffer — device is VK_NULL_HANDLE");
        return 0;
    }

    VkBufferUsageFlags fixedUsage = usage;

    if (fixedUsage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)) {
        fixedUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        LOG_INFO_CAT("BUFFER", "Auto-added SHADER_DEVICE_ADDRESS_BIT (fixed usage now 0x{:x})", fixedUsage);
    }

    if (fixedUsage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) {
        LOG_INFO_CAT("BUFFER", "Routing to descriptor buffer path");
        uint64_t handle = createDescriptorBuffer(size, tag);

        if (handle == 0) {
            std::string msg = std::format("Descriptor buffer creation failed (size={}, tag='{}')", size, tag);
            vkh.checker(false, "createDescriptorBuffer", msg.c_str());
            return 0;
        }

        LOG_SUCCESS_CAT("BUFFER", "Descriptor buffer created successfully — handle={:016x}", handle);
        return handle;
    }

    VkDeviceSize needed = size + TINY_SAFETY_MARGIN;
    VkDeviceSize avail = availableToTake();

    if (avail < needed) {
        std::string msg = std::format("Insufficient VRAM — needed ~{} MB, available={} MB (tag='{}')",
                                      needed / (1024 * 1024), avail / (1024 * 1024), tag);
        vkh.checker(false, "VRAM availability check", msg.c_str());
        return 0;
    }

    LOG_INFO_CAT("BUFFER", "VRAM check passed — needed ~{} MB, available={} MB",
                 needed / (1024 * 1024), avail / (1024 * 1024));

    bool isPureStaging = (usage == VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!isPureStaging) {
        fixedUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bool isSBT = (fixedUsage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) ||
                 (tag.find("SBT") != std::string_view::npos);

    if (isSBT) {
        VkDeviceSize originalSize = size;
        size = std::max(size, SBT_MINIMUM_SIZE);
        size = align_up(size, SBT_ALIGNMENT);
        fixedUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        LOG_INFO_CAT("BUFFER", "SBT detected — adjusted size {} → {} bytes (alignment={}), fixed usage=0x{:x}",
                     originalSize, size, SBT_ALIGNMENT, fixedUsage);
    }

    bool smallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                        (fixedUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    if (smallUniform) {
        LOG_INFO_CAT("BUFFER", "Small uniform → dedicated host-visible persistent buffer");

        VkBufferCreateInfo bci{};
        bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size        = size;
        bci.usage       = fixedUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf = VK_NULL_HANDLE;
        vkh.checker(
            vkCreateBuffer(dev, &bci, nullptr, &buf),
            "vkCreateBuffer (small uniform)",
            "Failed to create small uniform buffer"
        );

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(dev, buf, &req);

        uint32_t memType = findMemoryType(req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkh.checker(memType != ~0u, "findMemoryType (small uniform)",
                    "No host-visible coherent memory for small uniform");

        VkMemoryAllocateFlagsInfo flags{};
        flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.pNext           = &flags;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        VkDeviceMemory mem = VK_NULL_HANDLE;
        vkh.checker(
            vkAllocateMemory(dev, &mai, nullptr, &mem),
            "vkAllocateMemory (small uniform)",
            "Failed to allocate small uniform memory"
        );

        vkh.checker(
            vkBindBufferMemory(dev, buf, mem, 0),
            "vkBindBufferMemory (small uniform)",
            "Failed to bind small uniform memory"
        );

        VkBufferDeviceAddressInfo addrInfo{};
        addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = buf;
        VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

        uint64_t handle = ++rtx().next_buffer_handle;

        BufferInfo info{};
        info.buffer        = buf;
        info.memory        = mem;
        info.size          = size;
        info.aligned       = req.alignment;
        info.offset        = 0;
        info.deviceAddress = addr;
        info.usage         = fixedUsage;
        info.tag           = std::string(tag);

        VkResult res = vkMapMemory(dev, mem, 0, size, 0, &info.mapped);
        vkh.checker(res, "vkMapMemory (small uniform persistent)",
                    "Failed to persistently map small uniform buffer");

        rtx().buffers.emplace(handle, std::move(info));

        LOG_SUCCESS_CAT("BUFFER", "Small uniform created & persistently mapped — handle={:016x}, size={} bytes, addr=0x{:x}", handle, size, addr);
        return handle;
    }

    LOG_INFO_CAT("BUFFER", "Device-local buffer — using chunked allocation (chunk size={})", DEFAULT_CHUNK_SIZE);

    VkDeviceSize remaining = size;
    uint64_t firstHandle = 0;

    while (remaining > 0) {
        VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, remaining);
        LOG_INFO_CAT("BUFFER", "Allocating chunk — size={} bytes, remaining={}", chunkSize, remaining);

        Chunk* chunk = createChunk(chunkSize, fixedUsage);

        if (chunk == nullptr) {
            std::string msg = std::format("createChunk failed for size={} (tag='{}')", chunkSize, tag);
            vkh.checker(false, "createChunk", msg.c_str());
            return 0;
        }

        uint64_t chunkHandle = ++rtx().next_buffer_handle;
        rtx().buffers.emplace(chunkHandle, BufferInfo{
            chunk->buffer, chunk->memory, chunkSize, chunk->size, chunk->head,
            chunk->baseAddr + chunk->head, nullptr, fixedUsage, std::string(tag) + "_chunk"
        });

        chunk->handles.push_back(chunkHandle);

        if (firstHandle == 0) firstHandle = chunkHandle;

        remaining -= chunkSize;
        LOG_INFO_CAT("BUFFER", "Chunk allocated — handle={:016x}, base addr=0x{:x}", chunkHandle, chunk->baseAddr + chunk->head);
    }

    LOG_SUCCESS_CAT("BUFFER", "Device-local buffer created successfully — first handle={:016x}, total size={} bytes", 
                    firstHandle, size);
    return firstHandle;
}

[[nodiscard]] inline VkDeviceAddress allocateScratch(VkDeviceSize requiredSize) noexcept {
    LOG_INFO_CAT("MEMORY", "Allocating scratch space — required={} bytes", requiredSize);

    VkDeviceSize total = 0;
    VkDeviceAddress baseAddr = 0;

    while (total < requiredSize) {
        VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, requiredSize - total);
        uint64_t chunkHandle = create(
            chunkSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_SCRATCH_BIT_KHR,
            "LAS_Scratch_Chunk");

        if (chunkHandle == 0) {
            std::string msg = std::format("Failed to allocate scratch chunk of {} bytes", chunkSize);
            vkh.checker(false, "create (scratch chunk)", msg.c_str());
            return 0;
        }

        std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(chunkHandle);

        if (it == rtx().buffers.end()) {
            std::string msg = std::format("Allocated scratch chunk {:016x} but not in buffers map", chunkHandle);
            vkh.checker(false, "scratch chunk map lookup", msg.c_str());
            return 0;
        }

        VkDeviceAddress chunkAddr = it->second.deviceAddress + it->second.offset;
        if (total == 0) baseAddr = chunkAddr;

        total += chunkSize;
        LOG_INFO_CAT("MEMORY", "Scratch chunk allocated — handle={:016x}, addr=0x{:x}, size={} bytes", chunkHandle, chunkAddr, chunkSize);
    }

    LOG_SUCCESS_CAT("MEMORY", "Scratch space allocated — total={} bytes, base addr=0x{:x}", total, baseAddr);
    return baseAddr;
}

[[nodiscard]] inline std::pair<VkBuffer, VkDeviceMemory> uploadToBuffer(
    uint64_t handle,
    const void* data,
    VkDeviceSize size,
    VkCommandBuffer cmd = VK_NULL_HANDLE
) noexcept {
    std::unordered_map<uint64_t, BufferInfo>& buffers = rtx().buffers;
    std::unordered_map<uint64_t, BufferInfo>::iterator it = buffers.find(handle);

    if (it == buffers.end()) {
        std::string msg = std::format("uploadToBuffer failed — invalid handle {:016x}", handle);
        vkh.checker(false, "uploadToBuffer handle lookup", msg.c_str());
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    BufferInfo& info = it->second;

    if (size > info.size) {
        std::string msg = std::format("uploadToBuffer failed — size {} exceeds buffer capacity {} (handle={:016x}, tag='{}')",
                                      size, info.size, handle, info.tag);
        vkh.checker(false, "upload size validation", msg.c_str());
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    if (info.buffer == VK_NULL_HANDLE) {
        std::string msg = std::format("uploadToBuffer failed — buffer is null (handle={:016x}, tag='{}')", handle, info.tag);
        vkh.checker(false, "Target buffer validity", msg.c_str());
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    if (info.mapped != nullptr) {
        std::memcpy(info.mapped, data, size);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    VkDevice dev = rtx().device;

    bool usingExternalCmd = (cmd != VK_NULL_HANDLE);
    VkCommandBuffer targetCmd = usingExternalCmd ? cmd : VK_NULL_HANDLE;

    VkDeviceSize stageSize = align_up(size, 256);

    LOG_INFO_CAT("BUFFER", "Creating transient staging buffer — size={} bytes", stageSize);

    VkBufferCreateInfo stageBci{};
    stageBci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stageBci.size        = stageSize;
    stageBci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    stageBci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stageBuf = VK_NULL_HANDLE;
    vkh.checker(
        vkCreateBuffer(dev, &stageBci, nullptr, &stageBuf),
        "vkCreateBuffer (transient staging)",
        "Transient staging vkCreateBuffer failed"
    );

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, stageBuf, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkh.checker(memType != ~0u, "findMemoryType (staging)",
                "No host-visible coherent memory type for transient staging");

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext           = &flags;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory stageMem = VK_NULL_HANDLE;
    vkh.checker(
        vkAllocateMemory(dev, &mai, nullptr, &stageMem),
        "vkAllocateMemory (transient staging)",
        "Transient staging vkAllocateMemory failed"
    );

    vkh.checker(
        vkBindBufferMemory(dev, stageBuf, stageMem, 0),
        "vkBindBufferMemory (transient staging)",
        "Failed to bind transient staging memory"
    );

    LOG_SUCCESS_CAT("BUFFER", "Transient staging buffer ready — size={} bytes", stageSize);

    void* ptr = nullptr;
    vkh.checker(
        vkMapMemory(dev, stageMem, 0, stageSize, 0, &ptr),
        "vkMapMemory (transient staging)",
        "vkMapMemory(transient staging) failed"
    );

    std::memcpy(ptr, data, size);
    vkUnmapMemory(dev, stageMem);

    LOG_INFO_CAT("BUFFER", "Data copied to transient staging — {} bytes", size);

    VkCommandBuffer copyTarget = targetCmd;

    if (!usingExternalCmd) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = rtx().transient_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkh.checker(
            vkAllocateCommandBuffers(dev, &allocInfo, &copyTarget),
            "vkAllocateCommandBuffers (owned upload)",
            "Failed to allocate owned upload cmd"
        );

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkh.checker(
            vkBeginCommandBuffer(copyTarget, &beginInfo),
            "vkBeginCommandBuffer (owned upload)",
            "Failed to begin owned upload cmd"
        );
    }

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = info.offset;
    copy.size      = size;

    vkCmdCopyBuffer(copyTarget, stageBuf, info.buffer, 1, &copy);

    LOG_INFO_CAT("BUFFER", "Copy command recorded — {} bytes from staging → target", size);

    VkFence fence = VK_NULL_HANDLE;
    if (!usingExternalCmd) {
        vkh.checker(
            vkEndCommandBuffer(copyTarget),
            "vkEndCommandBuffer (owned upload)",
            "Failed to end owned upload cmd"
        );

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkh.checker(
            vkCreateFence(dev, &fenceInfo, nullptr, &fence),
            "vkCreateFence (owned upload)",
            "Failed to create fence"
        );

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &copyTarget;

        vkh.checker(
            vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence),
            "vkQueueSubmit (owned upload)",
            "vkQueueSubmit failed"
        );

        VkResult res = vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        vkh.checker(res, "vkWaitForFences (owned upload)",
                    "vkWaitForFences failed");

        vkResetFences(dev, 1, &fence);
        vkDestroyFence(dev, fence, nullptr);
        vkFreeCommandBuffers(dev, rtx().transient_pool, 1, &copyTarget);

        vkFreeMemory(dev, stageMem, nullptr);
        vkDestroyBuffer(dev, stageBuf, nullptr);

        LOG_SUCCESS_CAT("BUFFER", "Owned upload complete — staging cleaned");
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    } else {
        LOG_INFO_CAT("BUFFER", "External cmd path — staging returned for caller cleanup after submit/wait");
        return {stageBuf, stageMem};
    }
}

inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    std::unordered_map<uint64_t, BufferInfo>& buffers = rtx().buffers;
    std::unordered_map<uint64_t, BufferInfo>::iterator it = buffers.find(handle);
    if (it == buffers.end()) {
        LOG_WARNING_CAT("MEMORY", "destroy called on invalid handle {:016x}", handle);
        return;
    }

    BufferInfo& info = it->second;
    VkDevice dev = rtx().device;

    LOG_INFO_CAT("MEMORY", "Destroying buffer handle={:016x} (tag='{}', size={} bytes)", handle, info.tag, info.size);

    if (info.mapped != nullptr) {
        vkUnmapMemory(dev, info.memory);
        info.mapped = nullptr;
    }
    vkDestroyBuffer(dev, info.buffer, nullptr);
    vkFreeMemory(dev, info.memory, nullptr);

    buffers.erase(it);

    LOG_SUCCESS_CAT("MEMORY", "Buffer destroyed — handle={:016x}", handle);
}

[[nodiscard]] inline BufferInfo* get(uint64_t handle) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) {
        LOG_ERROR_CAT("MEMORY", "get failed — invalid handle {:016x}", handle);
        return nullptr;
    }
    return &it->second;
}

[[nodiscard]] inline VkBuffer getBuffer(uint64_t handle) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) {
        LOG_ERROR_CAT("MEMORY", "getBuffer failed — invalid handle {:016x}", handle);
        return VK_NULL_HANDLE;
    }
    return it->second.buffer;
}

[[nodiscard]] inline VkDeviceAddress getDeviceAddress(uint64_t handle) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) {
        LOG_ERROR_CAT("MEMORY", "getDeviceAddress failed — invalid handle {:016x}", handle);
        return 0;
    }
    return it->second.deviceAddress;
}

inline void init() noexcept {
    rtx().vram_reality = measureReality();
}

} // namespace Memory

[[nodiscard]] inline VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance inst,
    VkSurfaceKHR surf,
    uint32_t* out_graphics_family  = nullptr,
    uint32_t* out_present_family   = nullptr,
    uint32_t* out_compute_family   = nullptr,
    uint32_t* out_transfer_family  = nullptr
) noexcept {
    uint32_t count = 0;
    vkh.checker(
        vkEnumeratePhysicalDevices(inst, &count, nullptr),
        "vkEnumeratePhysicalDevices (count)",
        "Failed to enumerate physical device count"
    );

    vkh.checker(count > 0, "Physical devices existence",
                "No Vulkan-capable physical devices found");

    std::vector<VkPhysicalDevice> devices(count);
    vkh.checker(
        vkEnumeratePhysicalDevices(inst, &count, devices.data()),
        "vkEnumeratePhysicalDevices (devices)",
        "Failed to enumerate physical devices"
    );

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
        vkh.checker(
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr),
            "vkEnumerateDeviceExtensionProperties (count)",
            "Failed to enumerate device extension count"
        );

        std::vector<VkExtensionProperties> exts(extCount);
        vkh.checker(
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data()),
            "vkEnumerateDeviceExtensionProperties (extensions)",
            "Failed to enumerate device extensions"
        );

        bool has_rt = false;
        bool has_all = true;
        for (const char* req : requiredDeviceExtensions) {
            bool found = false;
            for (const VkExtensionProperties& avail : exts) {
                if (strcmp(avail.extensionName, req) == 0) {
                    found = true;
                    if (strcmp(req, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0 ||
                        strcmp(req, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
                        has_rt = true;
                    }
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
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 100000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 10000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    score += 1000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            score += 100;

        if (has_rt) score += 50000;

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

    vkh.checker(selected != VK_NULL_HANDLE, "GPU selection",
                "No suitable GPU found with required queue families and extensions");

    rtx().physical = selected;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(selected, &props);

    uint32_t api_major = VK_VERSION_MAJOR(props.apiVersion);
    uint32_t api_minor = VK_VERSION_MINOR(props.apiVersion);
    uint32_t api_patch = VK_VERSION_PATCH(props.apiVersion);

    uint32_t driver_major = VK_VERSION_MAJOR(props.driverVersion);
    uint32_t driver_minor = VK_VERSION_MINOR(props.driverVersion);
    uint32_t driver_patch = VK_VERSION_PATCH(props.driverVersion);

    std::string uuid_hex;
    for (uint32_t i = 0; i < VK_UUID_SIZE; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", props.pipelineCacheUUID[i]);
        uuid_hex += buf;
        if (i == 3 || i == 5 || i == 7 || i == 9) uuid_hex += "-";
    }

    LOG_INFO_CAT("VULKAN", 
        "\nSelected physical device: {} \nDevice ID: {} \nVendor ID: 0x{} \nType: {} "
        "\nAPI version: {}.{}.{} \nDriver version: {}.{}.{} "
        "\nPipeline cache UUID: {} \nLocal VRAM estimate: {} GB",
        props.deviceName,
        props.deviceID,
        props.vendorID,
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete GPU" :
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated GPU" :
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "virtual GPU" :
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU device" : "unknown",
        api_major, api_minor, api_patch,
        driver_major, driver_minor, driver_patch,
        uuid_hex,
        best_memory / (1024ULL * 1024 * 1024));

    std::set<uint32_t> unique_families = {
        best_indices.graphics.value(),
        best_indices.present.value(),
        best_indices.compute.value()
    };
    if (best_indices.transfer.has_value()) unique_families.insert(best_indices.transfer.value());

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(unique_families.size());
    for (uint32_t fam : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        queue_infos.push_back(qci);
    }

    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress     = VK_TRUE;
    vk12.descriptorIndexing      = VK_TRUE;
    vk12.runtimeDescriptorArray  = VK_TRUE;

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

    uint32_t extCount = 0;
    vkh.checker(
        vkEnumerateDeviceExtensionProperties(selected, nullptr, &extCount, nullptr),
        "vkEnumerateDeviceExtensionProperties (optional count)",
        "Failed to enumerate optional device extensions count"
    );

    std::vector<VkExtensionProperties> exts(extCount);
    vkh.checker(
        vkEnumerateDeviceExtensionProperties(selected, nullptr, &extCount, exts.data()),
        "vkEnumerateDeviceExtensionProperties (optional)",
        "Failed to enumerate optional device extensions"
    );

    std::vector<const char*> enabledExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

    std::array<const char*, 1> optionalExtensions = {{
        VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME
    }};

    for (const char* opt : optionalExtensions) {
        bool found = false;
        for (const VkExtensionProperties& avail : exts) {
            if (strcmp(avail.extensionName, opt) == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            enabledExtensions.push_back(opt);
            LOG_INFO_CAT("VULKAN", "Enabling optional extension: {}", opt);
        } else {
            LOG_INFO_CAT("VULKAN", "Optional extension not supported: {}", opt);
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
    vkh.checker(
        vkCreateDevice(selected, &dev_ci, nullptr, &dev),
        "vkCreateDevice",
        "vkCreateDevice failed"
    );

    rtx().device = dev;

    rtx().graphics_family  = best_indices.graphics.value();
    rtx().present_family   = best_indices.present.value();
    rtx().compute_family   = best_indices.compute.value();
    rtx().transfer_family  = best_indices.transfer.value_or(best_indices.graphics.value());

    vkGetDeviceQueue(dev, rtx().graphics_family,  0, &rtx().graphics_queue);
    vkGetDeviceQueue(dev, rtx().present_family,   0, &rtx().present_queue);
    vkGetDeviceQueue(dev, rtx().compute_family,   0, &rtx().compute_queue);
    vkGetDeviceQueue(dev, rtx().transfer_family,  0, &rtx().transfer_queue);

    if (out_graphics_family)  *out_graphics_family  = rtx().graphics_family;
    if (out_present_family)   *out_present_family   = rtx().present_family;
    if (out_compute_family)   *out_compute_family   = rtx().compute_family;
    if (out_transfer_family)  *out_transfer_family  = rtx().transfer_family;

    LOG_SUCCESS_CAT("VULKAN", "Logical device created — ray tracing, compute queues, and descriptor buffer support enabled");

    Memory::init();

    return dev;
}

// Swapchain — pre-acquire two images, cache timing delta, present to cached image
struct Swapchain {
    struct Handle {
        VkSwapchainKHR value;
        Handle() : value(VK_NULL_HANDLE) {}
        explicit Handle(VkSwapchainKHR v) : value(v) {}
        VkSwapchainKHR get() const { return value; }
        void reset() { value = VK_NULL_HANDLE; }
        bool valid() const { return value != VK_NULL_HANDLE; }
    };

    inline static Handle swapchain_;
    inline static VkImage swapchainImage_ = VK_NULL_HANDLE;
    inline static VkImageView swapchainImageView_ = VK_NULL_HANDLE;
    inline static VkExtent2D swapchainExtent_{0, 0};
    inline static VkFormat swapchainFormat_{};
    inline static bool minimized_ = false;
    inline static bool directWriteEnabled = false;

    // Pre-acquired image index (we cache one)
    inline static uint32_t cachedImageIndex = UINT32_MAX;

    // Last known present target time (seconds since genesis)
    inline static double lastPresentTarget_s = 0.0;

    // Estimated display refresh interval in seconds (updated from actual present timing)
    inline static double estimatedRefreshInterval_s = 1.0 / 60.0;

    static void ensureReady(int w, int h) noexcept {
        if (w == 0 || h == 0) {
            minimized_ = true;
            return;
        }

        vkDeviceWaitIdle(rtx().device);
        createOrRecreateSwapchain(w, h, true);
    }

    static void createOrRecreateSwapchain(int w, int h, bool isRecreate) noexcept {
        vkh.checker(rtx().device != VK_NULL_HANDLE && rtx().physical != VK_NULL_HANDLE && rtx().surface != VK_NULL_HANDLE && w > 0 && h > 0,
                    "Swapchain creation params",
                    "Cannot create swapchain — invalid params or minimized");

        vkDeviceWaitIdle(rtx().device);
        vkQueueWaitIdle(rtx().present_queue);

        if (isRecreate) {
            if (rtx().views != VK_NULL_HANDLE) {
                vkDestroyImageView(rtx().device, rtx().views, nullptr);
                rtx().views = VK_NULL_HANDLE;
            }
            if (swapchain_.valid()) {
                ext().vkDestroySwapchainKHR(rtx().device, swapchain_.get(), nullptr);
                swapchain_.reset();
            }
            swapchainImage_ = VK_NULL_HANDLE;
            swapchainImageView_ = VK_NULL_HANDLE;
            rtx().images = VK_NULL_HANDLE;
            cachedImageIndex = UINT32_MAX; // invalidate cache
        }

        minimized_ = false;

        VkSurfaceCapabilitiesKHR caps{};
        vkh.checker(
            ext().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rtx().physical, rtx().surface, &caps),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
            "Failed to get surface capabilities"
        );

        VkExtent2D extent{};
        if (caps.currentExtent.width == UINT32_MAX) {
            extent.width  = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
        } else {
            extent = caps.currentExtent;
        }

        vkh.checker(extent.width > 0 && extent.height > 0, "Swapchain extent validation",
                    "Swapchain extent zero — window minimized");

        uint32_t fmtCount = 0;
        vkh.checker(
            ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR (count)",
            "Failed to get surface format count"
        );

        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkh.checker(
            ext().vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &fmtCount, formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR (formats)",
            "Failed to get surface formats"
        );

        VkSurfaceFormatKHR chosenFormat = formats.empty() ? VkSurfaceFormatKHR{} : formats.front();

        constexpr std::array preferredFormats = {
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        };

        for (const VkSurfaceFormatKHR& pref : preferredFormats) {
            auto it = std::find_if(formats.begin(), formats.end(),
                                   [&](const VkSurfaceFormatKHR& f) { return f.format == pref.format && f.colorSpace == pref.colorSpace; });
            if (it != formats.end()) {
                chosenFormat = *it;
                break;
            }
        }

        LOG_INFO_CAT("SWAPCHAIN", "Chosen format: {} ({})",
                     vkh.format(chosenFormat.format),
                     vkh.colorspace(chosenFormat.colorSpace));

        uint32_t pmCount = 0;
        vkh.checker(
            ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, nullptr),
            "vkGetPhysicalDeviceSurfacePresentModesKHR (count)",
            "Failed to get present mode count"
        );

        std::vector<VkPresentModeKHR> supportedModes(pmCount);
        vkh.checker(
            ext().vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &pmCount, supportedModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR (modes)",
            "Failed to get present modes"
        );

        VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;

        if (std::find(supportedModes.begin(), supportedModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != supportedModes.end()) {
            chosenPM = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_SUCCESS_CAT("SWAPCHAIN", "IMMEDIATE selected (tearing OK, lowest latency)");
        } else if (std::find(supportedModes.begin(), supportedModes.end(), VK_PRESENT_MODE_FIFO_LATEST_READY_EXT) != supportedModes.end()) {
            chosenPM = VK_PRESENT_MODE_FIFO_LATEST_READY_EXT;
            LOG_INFO_CAT("SWAPCHAIN", "Using FIFO_LATEST_READY (best effort low-latency VSync)");
        } else if (std::find(supportedModes.begin(), supportedModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != supportedModes.end()) {
            chosenPM = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_INFO_CAT("SWAPCHAIN", "MAILBOX selected (triple buffering, good tear-free low latency)");
        } else {
            LOG_WARNING_CAT("SWAPCHAIN", "Falling back to standard FIFO (guaranteed VSync, higher latency)");
        }

        uint32_t imgCount = caps.minImageCount;
        if (chosenPM == VK_PRESENT_MODE_FIFO_KHR || chosenPM == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT) {
            imgCount = std::max(caps.minImageCount, 2u);
            imgCount = std::min(imgCount, caps.maxImageCount ? caps.maxImageCount : imgCount);
        }

        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
            VkImageFormatProperties props{};
            VkResult formatRes = vkGetPhysicalDeviceImageFormatProperties(
                rtx().physical,
                chosenFormat.format,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_STORAGE_BIT,
                0,
                &props
            );

            if (formatRes == VK_SUCCESS) {
                directWriteEnabled = true;
                usage |= VK_IMAGE_USAGE_STORAGE_BIT;
                LOG_SUCCESS_CAT("SWAPCHAIN", "STORAGE usage ENABLED — direct render to swapchain possible");
            } else {
                LOG_WARNING_CAT("SWAPCHAIN", "STORAGE usage NOT supported — query returned {}", vkh.result(formatRes));
                directWriteEnabled = false;
            }
        } else {
            LOG_WARNING_CAT("SWAPCHAIN", "Surface does not support STORAGE usage — cannot direct render");
            directWriteEnabled = false;
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface         = rtx().surface;
        ci.minImageCount   = imgCount;
        ci.imageFormat     = chosenFormat.format;
        ci.imageColorSpace = chosenFormat.colorSpace;
        ci.imageExtent     = extent;
        ci.imageArrayLayers = 1;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform    = caps.currentTransform;
        ci.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode     = chosenPM;
        ci.clipped         = VK_TRUE;
        ci.oldSwapchain    = swapchain_.get();
        ci.imageUsage      = usage;

        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        vkh.checker(
            ext().vkCreateSwapchainKHR(rtx().device, &ci, nullptr, &newSwap),
            "vkCreateSwapchainKHR",
            "vkCreateSwapchainKHR failed"
        );

        swapchain_ = Handle(newSwap);
        swapchainExtent_ = extent;
        swapchainFormat_ = chosenFormat.format;

        uint32_t count = 0;
        vkh.checker(
            ext().vkGetSwapchainImagesKHR(rtx().device, newSwap, &count, nullptr),
            "vkGetSwapchainImagesKHR (count)",
            "Failed to get swapchain image count"
        );

        VkImage singleImage = VK_NULL_HANDLE;
        vkh.checker(
            ext().vkGetSwapchainImagesKHR(rtx().device, newSwap, &count, &singleImage),
            "vkGetSwapchainImagesKHR (single image)",
            "Failed to get swapchain image"
        );

        VkImageViewCreateInfo viewCI{};
        viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format           = chosenFormat.format;
        viewCI.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        viewCI.image            = singleImage;

        VkImageView singleView = VK_NULL_HANDLE;
        vkh.checker(
            vkCreateImageView(rtx().device, &viewCI, nullptr, &singleView),
            "vkCreateImageView (swapchain image)",
            "Failed to create swapchain image view"
        );

        swapchainImage_      = singleImage;
        swapchainImageView_  = singleView;
        rtx().images         = singleImage;
        rtx().views          = singleView;
        rtx().extent         = extent;
        rtx().image_count    = 1;

        // Reset timing state on recreate
        lastPresentTarget_s = 0.0;
        estimatedRefreshInterval_s = 1.0 / 60.0;

        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain {} — single image, {}x{}, format {}, mode {}, direct-write={}",
                        isRecreate ? "recreated" : "created",
                        extent.width, extent.height,
                        vkh.format(chosenFormat.format),
                        chosenPM == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
                        chosenPM == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT ? "FIFO_LATEST_READY" :
                        chosenPM == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO",
                        directWriteEnabled ? "ENABLED" : "disabled");
    }

    // Decide if we should present NOW based on TotalTime vs last present + refresh estimate
    static bool shouldPresentNow() noexcept {
        double now_s = TotalTime::get().seconds();

        // First frame — always present
        if (lastPresentTarget_s <= 0.0) {
            lastPresentTarget_s = now_s;
            return true;
        }

        // Calculate next expected vsync time
        double nextVsync_s = lastPresentTarget_s + estimatedRefreshInterval_s;

        // If we're already past the next vsync window, present immediately (catch-up)
        if (now_s >= nextVsync_s) {
            lastPresentTarget_s = now_s;
            return true;
        }

        // Otherwise wait — we're early for this frame
        return false;
    }

    // Update estimated refresh interval based on actual present-to-present delta
    static void updateRefreshEstimate(double now_s) noexcept {
        if (lastPresentTarget_s > 0.0) {
            double delta = now_s - lastPresentTarget_s;
            if (delta > 0.001 && delta < 0.1) {
                estimatedRefreshInterval_s = 0.8 * estimatedRefreshInterval_s + 0.2 * delta;
                LOG_PERF_CAT("SWAPCHAIN", "Updated refresh estimate: {:.4f} s ({:.1f} Hz)",
                             estimatedRefreshInterval_s, 1.0 / estimatedRefreshInterval_s);
            }
        }
        lastPresentTarget_s = now_s;
    }

    static VkResult presentImage(VkQueue queue) noexcept {
        if (minimized_ || !swapchain_.valid()) {
            return VK_SUCCESS;
        }

        double now_s = TotalTime::get().seconds();

        // Only present if we're in/after the vsync window
        if (!shouldPresentNow()) {
            return VK_EVENT_SET; // not ready yet
        }

        VkSwapchainKHR sw = swapchain_.get();

        uint32_t imageIndex = 0; // always 0 — single image

        VkPresentInfoKHR pi{};
        pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &sw;
        pi.pImageIndices      = &imageIndex;

        VkResult res = ext().vkQueuePresentKHR(queue, &pi);

        if (res == VK_SUCCESS) {
            updateRefreshEstimate(now_s);
            LOG_INFO_CAT("SWAPCHAIN", "Present successful — single image @ {:.6f} s", now_s);
        } else if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            minimized_ = true;
            LOG_WARNING_CAT("SWAPCHAIN", "Present returned out-of-date/suboptimal — minimized set");
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "vkQueuePresentKHR failed: {}", vkh.result(res));
        }

        return res;
    }

    static void recreate(int width, int height) noexcept {
        vkDeviceWaitIdle(rtx().device);
        createOrRecreateSwapchain(width, height, true);
    }

    static void create(SDL_Window* window, int width, int height) noexcept {
        rtx().window = window;
        createOrRecreateSwapchain(width, height, false);
    }

    static void cleanup() noexcept {
        VkDevice dev = rtx().device;
        if (dev == VK_NULL_HANDLE) return;

        vkDeviceWaitIdle(dev);
        vkQueueWaitIdle(rtx().present_queue);

        if (swapchainImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, swapchainImageView_, nullptr);
            swapchainImageView_ = VK_NULL_HANDLE;
        }

        if (swapchain_.valid()) {
            ext().vkDestroySwapchainKHR(dev, swapchain_.get(), nullptr);
            swapchain_.reset();
        }

        swapchainImage_ = VK_NULL_HANDLE;
        lastPresentTarget_s = 0.0;
        estimatedRefreshInterval_s = 1.0 / 60.0;
    }
};

// Vertex structure — only used for loading AABB bounds (no triangle rendering)
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Mesh structure — only for loading OBJ to extract AABB
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 aabbMin{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 aabbMax{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    void computeAABB() noexcept {
        if (vertices.empty()) return;

        aabbMin = vertices[0].pos;
        aabbMax = vertices[0].pos;

        for (const Vertex& v : vertices) {
            aabbMin = glm::min(aabbMin, v.pos);
            aabbMax = glm::max(aabbMax, v.pos);
        }

        glm::vec3 padding = (aabbMax - aabbMin) * 0.001f;
        aabbMin -= padding;
        aabbMax += padding;
    }
};

// Load OBJ mesh — only to extract AABB, no vertex/index buffers kept
[[nodiscard]] inline std::unique_ptr<Mesh> loadOBJ(std::string_view path) noexcept {
    LOG_ATTEMPT_CAT("LAS", "Loading OBJ as AABB: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string baseDir = "assets/models/";
    bool loaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, std::string(path).c_str(), baseDir.c_str());
    vkh.checker(loaded, "tinyobj::LoadObj",
                std::format("Failed to load OBJ: {}", err.empty() ? "Unknown error" : err).c_str());

    if (!warn.empty()) LOG_WARNING_CAT("LAS", "{}", warn);

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    glm::vec3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            if (index.vertex_index < 0) continue;

            size_t base = static_cast<size_t>(3 * index.vertex_index);
            glm::vec3 pos{
                attrib.vertices[base + 0],
                attrib.vertices[base + 1],
                attrib.vertices[base + 2]
            };

            min = glm::min(min, pos);
            max = glm::max(max, pos);
        }
    }

    vkh.checker(glm::all(glm::greaterThan(max, min)), "OBJ AABB computation",
                "Failed to compute valid AABB from OBJ");

    mesh->aabbMin = min;
    mesh->aabbMax = max;

    glm::vec3 padding = (max - min) * 0.001f;
    mesh->aabbMin -= padding;
    mesh->aabbMax += padding;

    LOG_SUCCESS_CAT("LAS", "OBJ loaded as AABB — min: ({},{},{}) | max: ({},{},{})",
                    min.x, min.y, min.z, max.x, max.y, max.z);

    return mesh;
}

// Create plane mesh — only AABB
[[nodiscard]] inline std::unique_ptr<Mesh> createPlane(float width = 1000.0f, float depth = 1000.0f) noexcept {
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;

    mesh->aabbMin = {-halfW, -0.01f, -halfD};
    mesh->aabbMax = { halfW,  0.01f,  halfD};

    LOG_SUCCESS_CAT("LAS", "Procedural plane AABB created — width: {} | depth: {}", width, depth);

    return mesh;
}

// Create billboard mesh — only AABB
[[nodiscard]] inline std::unique_ptr<Mesh> createBillboard() noexcept {
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    mesh->aabbMin = {-0.5f, -0.5f, -0.01f};
    mesh->aabbMax = { 0.5f,  0.5f,  0.01f};

    LOG_SUCCESS_CAT("LAS", "Procedural billboard AABB created — sacred pink quad");

    return mesh;
}

// Add AABB from mesh (only AABB, no vertex/index buffers)
inline size_t addAABBFromMesh(std::unique_ptr<Mesh> mesh, uint32_t materialIndex = 0,
                              const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    if (mesh == nullptr) return rtx().las_procedural_primitives.size();

    mesh->computeAABB();

    UniversalPrimitive p{};
    p.aabbMin = glm::vec4(mesh->aabbMin, 0.0f);
    p.aabbMax = glm::vec4(mesh->aabbMax, 0.0f);
    p.transform = transform;
    p.type = 0;
    p.materialIndex = materialIndex;
    p.destruction = 0.0f;

    rtx().las_procedural_primitives.push_back(p);
    rtx().las_procedural_dirty = true;
    rtx().las_tlas_dirty = true;

    LOG_SUCCESS_CAT("LAS", "Mesh converted to AABB — min: ({},{},{}) | max: ({},{},{}) | material: {}",
                    mesh->aabbMin.x, mesh->aabbMin.y, mesh->aabbMin.z,
                    mesh->aabbMax.x, mesh->aabbMax.y, mesh->aabbMax.z,
                    materialIndex);

    return rtx().las_procedural_primitives.size() - 1;
}

// Add procedural AABB
inline size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                                uint32_t materialIndex = 0,
                                const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    UniversalPrimitive p{};
    p.aabbMin = glm::vec4(center - glm::vec3(scale), 0.0f);
    p.aabbMax = glm::vec4(center + glm::vec3(scale), 0.0f);
    p.transform = transform;
    p.type = static_cast<uint32_t>(type);
    p.materialIndex = materialIndex;
    p.destruction = 0.0f;

    rtx().las_procedural_primitives.push_back(p);
    rtx().las_procedural_dirty = true;
    rtx().las_tlas_dirty = true;

    LOG_INFO_CAT("LAS", "Procedural AABB added — type {}, scale {}, material {}", 
                 static_cast<int>(type), scale, materialIndex);

    return rtx().las_procedural_primitives.size() - 1;
}

// Create default hybrid scene — pure procedural AABBs
inline void createDefaultHybridScene() noexcept {
    auto ground = createPlane(5000.0f, 5000.0f);
    addAABBFromMesh(std::move(ground), 0);

    auto billboard = createBillboard();
    addAABBFromMesh(std::move(billboard), 1);

    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(0, 5, 0), 2.0f, 2);
    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(4, 5, 4), 1.5f, 3);
    addProceduralAABB(GeometryType::ProceduralSphere, glm::vec3(-4, 5, -4), 1.5f, 4);

    float ringRadius = 10.0f;
    for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(i) * (3.14159f * 2.0f / 6.0f);
        glm::vec3 pos(std::cos(angle) * ringRadius, 3.0f, std::sin(angle) * ringRadius);
        addProceduralAABB(GeometryType::ProceduralD6, pos, 2.0f, static_cast<uint32_t>(5 + i),
                          glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0)));
    }

    addProceduralAABB(GeometryType::ProceduralD100, glm::vec3(0, 7, -14), 4.0f, 11,
                      glm::rotate(glm::mat4(1.0f), 0.25f, glm::vec3(0,1,0)));

    addProceduralAABB(GeometryType::ProceduralCylinder, glm::vec3(-15, 10, -15), 2.0f, 6);
    addProceduralAABB(GeometryType::ProceduralCone, glm::vec3(0, 15, 0), 5.0f, 7);

    LOG_SUCCESS_CAT("LAS", "Default hybrid AABB scene created — {} procedurals", rtx().las_procedural_primitives.size());
}

// On resize handler
inline void onResize() noexcept {
    rtx().las_tlas_dirty = true;
    rtx().las_procedural_dirty = true;
    LOG_INFO_CAT("LAS", "Resize detected — marked dirty for rebuild");
}

// Convert GLM mat4 to VkTransformMatrixKHR
inline VkTransformMatrixKHR to_vk_transform(const glm::mat4& m) noexcept {
    VkTransformMatrixKHR vkMat{};

    vkMat.matrix[0][0] = m[0][0]; vkMat.matrix[0][1] = m[1][0]; vkMat.matrix[0][2] = m[2][0]; vkMat.matrix[0][3] = m[3][0];
    vkMat.matrix[1][0] = m[0][1]; vkMat.matrix[1][1] = m[1][1]; vkMat.matrix[1][2] = m[2][1]; vkMat.matrix[1][3] = m[3][1];
    vkMat.matrix[2][0] = m[0][2]; vkMat.matrix[2][1] = m[1][2]; vkMat.matrix[2][2] = m[2][2]; vkMat.matrix[2][3] = m[3][2];

    return vkMat;
}

inline void ensureReady(VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    if (Swapchain::minimized_ || !Swapchain::swapchain_.valid()) {
        LOG_WARNING_CAT("LAS", "Swapchain minimized or invalid — skipping Level Acceleration Structure rebuild");
        return;
    }

    if (rtx().las_initialized &&
        !rtx().las_tlas_dirty &&
        !rtx().las_procedural_dirty &&
        rtx().las_tlas != VK_NULL_HANDLE)
    {
        LOG_INFO_CAT("LAS", "Level Acceleration Structure is fully up to date — no rebuild required");
        return;
    }

    LOG_INFO_CAT("LAS", "Level Acceleration Structure rebuild triggered — state: top-level={}, procedural={}",
                 rtx().las_tlas_dirty ? "dirty" : "clean",
                 rtx().las_procedural_dirty ? "dirty" : "clean");

    if (!rtx().las_initialized) {
        LOG_INFO_CAT("LAS", "First initialization — building default hybrid AABB scene");
        createDefaultHybridScene();
        rtx().las_initialized = true;
    }

    VkCommandBuffer localCmd = cmd;
    bool ownsCmd = (cmd == VK_NULL_HANDLE);

    if (ownsCmd) {
        vkh.checker(rtx().transient_pool != VK_NULL_HANDLE, "Transient pool",
                    "No transient command pool — cannot rebuild Level Acceleration Structure");

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = rtx().transient_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkh.checker(
            vkAllocateCommandBuffers(rtx().device, &allocInfo, &localCmd),
            "vkAllocateCommandBuffers",
            "Failed to allocate command buffer for rebuild"
        );

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkh.checker(
            vkBeginCommandBuffer(localCmd, &beginInfo),
            "vkBeginCommandBuffer",
            "Failed to begin command buffer for rebuild"
        );

        LOG_INFO_CAT("LAS", "Started fresh temporary command buffer for Level Acceleration Structure rebuild");
    } else {
        LOG_INFO_CAT("LAS", "Recording Level Acceleration Structure rebuild into provided command buffer");
    }

    std::vector<std::pair<VkBuffer, VkDeviceMemory>> pendingStaging;

    auto safeUpload = [&](uint64_t bufHandle, const void* srcData, VkDeviceSize uploadSize, const std::string& tag) {
        auto staging = Memory::uploadToBuffer(bufHandle, srcData, uploadSize, localCmd);
        LOG_INFO_CAT("LAS", "Upload queued for {}", tag);
        pendingStaging.push_back(staging);
    };

    if (rtx().las_procedural_dirty) {
        LOG_INFO_CAT("Procedural", "Procedural geometry changed — refreshing on-device buffer");

        VkDeviceSize primSize = rtx().las_procedural_primitives.size() * sizeof(UniversalPrimitive);
        if (primSize == 0) primSize = 16;

        if (rtx().las_universal_primitives_buffer == 0) {
            uint64_t primHandle = Memory::create(primSize,
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 "LAS_UniversalPrimitives");

            vkh.checker(primHandle != 0, "Memory::create", "Procedural buffer allocation failed");
            rtx().las_universal_primitives_buffer = primHandle;
        }

        safeUpload(rtx().las_universal_primitives_buffer,
                   rtx().las_procedural_primitives.data(),
                   primSize, "procedural primitives data");

        LOG_SUCCESS_CAT("Procedural", "Uploaded {} procedural primitives ({} bytes)",
                        rtx().las_procedural_primitives.size(), primSize);
        rtx().las_procedural_dirty = false;
    }

    if (rtx().las_tlas_dirty) {
        LOG_INFO_CAT("TLAS", "Rebuilding top-level acceleration structure — {} procedural instances",
                     rtx().las_procedural_primitives.size());

        VkDeviceSize instanceCount = rtx().las_procedural_primitives.size();
        VkDeviceSize instanceSize = instanceCount * sizeof(VkAccelerationStructureInstanceKHR);
        if (instanceSize == 0) instanceSize = 64;

        if (rtx().las_instance_buffer == 0) {
            uint64_t instHandle = Memory::create(instanceSize,
                                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 "LAS_InstanceBuffer");

            vkh.checker(instHandle != 0, "Memory::create", "Top-level instance buffer allocation failed");
            rtx().las_instance_buffer = instHandle;
        }

        std::vector<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(instanceCount);

        uint32_t instanceId = 0;

        for (const auto& prim : rtx().las_procedural_primitives) {
            VkAccelerationStructureInstanceKHR inst{};
            inst.transform = to_vk_transform(prim.transform);
            inst.instanceCustomIndex = instanceId++ & 0xFFFFFFu;
            inst.mask = 0xFF;
            inst.instanceShaderBindingTableRecordOffset = 0;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference = 0;

            instances.push_back(inst);
        }

        auto staging = Memory::uploadToBuffer(
            rtx().las_instance_buffer,
            instances.data(),
            instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
            localCmd
        );
        pendingStaging.push_back(staging);

        LOG_SUCCESS_CAT("TLAS", "Uploaded {} instances to top-level buffer", instanceCount);

        VkAccelerationStructureGeometryKHR tlasGeom{};
        tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
        tlasGeom.geometry.instances.data.deviceAddress = Memory::getDeviceAddress(rtx().las_instance_buffer);

        VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{};
        tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        tlasBuild.geometryCount = 1;
        tlasBuild.pGeometries = &tlasGeom;

        VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
        tlasRange.primitiveCount = static_cast<uint32_t>(instanceCount);
        tlasRange.primitiveOffset = 0;
        tlasRange.firstVertex = 0;
        tlasRange.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

        VkAccelerationStructureBuildSizesInfoKHR tlasSizes{};
        tlasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        ext().vkGetAccelerationStructureBuildSizesKHR(
            rtx().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &tlasBuild, &tlasRange.primitiveCount, &tlasSizes);

        VkDeviceAddress tlasScratchAddr = Memory::allocateScratch(tlasSizes.buildScratchSize);
        if (tlasScratchAddr == 0) {
            LOG_ERROR_CAT("TLAS", "Failed to allocate scratch memory for top-level rebuild");
        } else {
            tlasBuild.scratchData.deviceAddress = tlasScratchAddr;
        }

        if (rtx().las_tlas == VK_NULL_HANDLE) {
            uint64_t tlasStorageHandle = Memory::create(tlasSizes.accelerationStructureSize,
                                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                        "LAS_TLAS_Storage");

            vkh.checker(tlasStorageHandle != 0, "Memory::create", "Top-level storage allocation failed");
            rtx().las_tlas_storage = tlasStorageHandle;

            BufferInfo* storage = Memory::get(tlasStorageHandle);
            vkh.checker(storage != nullptr, "Memory::get", "Failed to access top-level storage");

            VkAccelerationStructureCreateInfoKHR tlasCreate{};
            tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            tlasCreate.type  = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            tlasCreate.buffer = storage->buffer;
            tlasCreate.size   = tlasSizes.accelerationStructureSize;

            vkh.checker(
                ext().vkCreateAccelerationStructureKHR(rtx().device, &tlasCreate, nullptr, &rtx().las_tlas),
                "vkCreateAccelerationStructureKHR",
                "Failed to create top-level acceleration structure"
            );

            LOG_SUCCESS_CAT("TLAS", "Created fresh top-level acceleration structure");
        }

        tlasBuild.dstAccelerationStructure = rtx().las_tlas;

        ext().vkCmdBuildAccelerationStructuresKHR(localCmd, 1, &tlasBuild, &pTlasRange);

        VkMemoryBarrier tlasBarrier{};
        tlasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        tlasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        tlasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        vkCmdPipelineBarrier(localCmd,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &tlasBarrier, 0, nullptr, 0, nullptr);

        rtx().las_tlas_dirty = false;
        LOG_SUCCESS_CAT("TLAS", "Top-level acceleration structure rebuilt with {} instances", instanceCount);
    }

    if (ownsCmd) {
        vkh.checker(
            vkEndCommandBuffer(localCmd),
            "vkEndCommandBuffer",
            "Failed to finalize rebuild command buffer"
        );

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &localCmd;

        vkh.checker(
            vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE),
            "vkQueueSubmit",
            "Failed to submit rebuild commands"
        );

        vkQueueWaitIdle(rtx().graphics_queue);

        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &localCmd);

        for (auto& p : pendingStaging) {
            if (p.first != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, p.first, nullptr);
                vkFreeMemory(rtx().device, p.second, nullptr);
            }
        }
        pendingStaging.clear();

        LOG_SUCCESS_CAT("LAS", "Level Acceleration Structure rebuild complete — structures updated, resources cleaned");
    } else {
        LOG_INFO_CAT("LAS", "Level Acceleration Structure rebuild recorded — awaiting submission from caller");

        if (!pendingStaging.empty()) {
            LOG_WARNING_CAT("LAS", "{} staging buffers still pending — cleanup required after queue wait",
                            pendingStaging.size());
        } else {
            LOG_INFO_CAT("LAS", "No staging buffers created in this rebuild");
        }
    }

    LOG_SUCCESS_CAT("LAS", "Level Acceleration Structure update finished");
}

// Get top-level acceleration structure
[[nodiscard]] inline VkAccelerationStructureKHR getTLAS() noexcept {
    ensureReady();
    return rtx().las_tlas;
}