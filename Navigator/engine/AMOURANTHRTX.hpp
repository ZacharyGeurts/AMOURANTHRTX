#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Header-Only Hybrid 2026 Edition
// Pure raymarching + hardware ray tracing + procedural geometry
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <set>
#include <algorithm>
#include <cstring>

// Required device extensions
inline constexpr std::array<const char*, 9> requiredDeviceExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
}};

// ────────────────────────────────────────────────
// Procedural geometry
// ────────────────────────────────────────────────

enum class GeometryType : uint32_t {
    ProceduralPlane      = 0,
    ProceduralSphere     = 1,
    ProceduralCylinder   = 2,
    ProceduralCone       = 3,
    ProceduralWaterPlane = 4,
};

struct alignas(16) UniversalPrimitive {
    glm::vec4   aabbMin;
    glm::vec4   aabbMax;
    glm::mat4   transform;
    uint32_t    type            = 0;
    uint32_t    materialIndex   = 0;
    float       destruction     = 0.0f;
    float       pad0            = 0.0f;
};

// ────────────────────────────────────────────────
// VRAM tracking (for RayCanvas log)
// ────────────────────────────────────────────────
struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = 256ULL << 20;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
    VkDeviceSize max_alloc_size   = 0;
    uint32_t     max_alloc_count  = 0;
};

// ────────────────────────────────────────────────
// Core context - rtx().everything
// ────────────────────────────────────────────────
struct RTX {
    VkInstance                      instance            = VK_NULL_HANDLE;
    VkPhysicalDevice                physical            = VK_NULL_HANDLE;
    VkDevice                        device              = VK_NULL_HANDLE;
    VkSurfaceKHR                    surface             = VK_NULL_HANDLE;

    VkQueue                         graphics_queue      = VK_NULL_HANDLE;
    VkQueue                         present_queue       = VK_NULL_HANDLE;
    VkQueue                         compute_queue       = VK_NULL_HANDLE;
    VkQueue                         transfer_queue      = VK_NULL_HANDLE;

    uint32_t                        graphics_family     = ~0u;
    uint32_t                        present_family      = ~0u;
    uint32_t                        compute_family      = ~0u;
    uint32_t                        transfer_family     = ~0u;

    SDL_Window*                     window              = nullptr;

    VkCommandPool                   transient_pool      = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
    bool                            rt_props_cached     = false;

    VkPipeline                      compute_pipeline    = VK_NULL_HANDLE;
    VkPipeline                      rt_pipeline         = VK_NULL_HANDLE;
    VkPipelineLayout                pipeline_layout     = VK_NULL_HANDLE;

    uint64_t                        las_universal_primitives_buffer = 0;
    uint64_t                        las_aabb_buffer     = 0;
    VkAccelerationStructureKHR      las_as              = VK_NULL_HANDLE;
    uint64_t                        las_as_storage      = 0;
    std::vector<UniversalPrimitive> las_procedural_primitives;
    bool                            las_initialized     = false;
    bool                            las_dirty           = true;

    VkDeviceAddress                 sbt_address         = 0;
    VkDeviceSize                    sbt_size            = 0;
    VkStridedDeviceAddressRegionKHR raygen_sbt_region{};
    VkStridedDeviceAddressRegionKHR miss_sbt_region{};
    VkStridedDeviceAddressRegionKHR hit_sbt_region{};
    VkStridedDeviceAddressRegionKHR callable_sbt_region{};

    struct BufferInfo {
        VkBuffer            buffer          = VK_NULL_HANDLE;
        VkDeviceMemory      memory          = VK_NULL_HANDLE;
        VkDeviceSize        size            = 0;
        VkDeviceAddress     deviceAddress   = 0;
        void*               mapped          = nullptr;
        VkBufferUsageFlags  usage           = 0;
        std::string         tag;
    };

    std::unordered_map<uint64_t, BufferInfo> buffers;
    uint64_t                        next_buffer_handle  = 1ULL;
    std::mutex                      buffer_mutex;
};

// RIP BW 🧑🏾‍🩰
inline RTX& rtx() noexcept {
    static RTX ctx;
    return ctx;
}

// ────────────────────────────────────────────────
// Extension loader
// ────────────────────────────────────────────────
struct VulkanExtensions {
    PFN_vkCreateSwapchainKHR                        vkCreateSwapchainKHR{};
    PFN_vkDestroySwapchainKHR                       vkDestroySwapchainKHR{};
    PFN_vkGetSwapchainImagesKHR                     vkGetSwapchainImagesKHR{};
    PFN_vkAcquireNextImageKHR                       vkAcquireNextImageKHR{};
    PFN_vkQueuePresentKHR                           vkQueuePresentKHR{};

    PFN_vkCreateRayTracingPipelinesKHR              vkCreateRayTracingPipelinesKHR{};
    PFN_vkGetRayTracingShaderGroupHandlesKHR        vkGetRayTracingShaderGroupHandlesKHR{};
    PFN_vkCmdTraceRaysKHR                           vkCmdTraceRaysKHR{};

    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR{};
    PFN_vkCmdBuildAccelerationStructuresKHR         vkCmdBuildAccelerationStructuresKHR{};
    PFN_vkCreateAccelerationStructureKHR            vkCreateAccelerationStructureKHR{};
    PFN_vkDestroyAccelerationStructureKHR           vkDestroyAccelerationStructureKHR{};
    PFN_vkGetAccelerationStructureDeviceAddressKHR  vkGetAccelerationStructureDeviceAddressKHR{};

    PFN_vkGetBufferDeviceAddress                    vkGetBufferDeviceAddress{};

    PFN_vkCmdBeginRendering                         vkCmdBeginRendering{};
    PFN_vkCmdEndRendering                           vkCmdEndRendering{};
};

inline VulkanExtensions& ext() noexcept {
    static VulkanExtensions e;
    static bool loaded = false;

    if (!loaded && rtx().device) {
        e.vkCreateSwapchainKHR                      = (PFN_vkCreateSwapchainKHR)                     vkGetDeviceProcAddr(rtx().device, "vkCreateSwapchainKHR");
        e.vkDestroySwapchainKHR                     = (PFN_vkDestroySwapchainKHR)                    vkGetDeviceProcAddr(rtx().device, "vkDestroySwapchainKHR");
        e.vkGetSwapchainImagesKHR                   = (PFN_vkGetSwapchainImagesKHR)                  vkGetDeviceProcAddr(rtx().device, "vkGetSwapchainImagesKHR");
        e.vkAcquireNextImageKHR                     = (PFN_vkAcquireNextImageKHR)                    vkGetDeviceProcAddr(rtx().device, "vkAcquireNextImageKHR");
        e.vkQueuePresentKHR                         = (PFN_vkQueuePresentKHR)                        vkGetDeviceProcAddr(rtx().device, "vkQueuePresentKHR");

        e.vkCreateRayTracingPipelinesKHR            = (PFN_vkCreateRayTracingPipelinesKHR)           vkGetDeviceProcAddr(rtx().device, "vkCreateRayTracingPipelinesKHR");
        e.vkGetRayTracingShaderGroupHandlesKHR      = (PFN_vkGetRayTracingShaderGroupHandlesKHR)     vkGetDeviceProcAddr(rtx().device, "vkGetRayTracingShaderGroupHandlesKHR");
        e.vkCmdTraceRaysKHR                         = (PFN_vkCmdTraceRaysKHR)                        vkGetDeviceProcAddr(rtx().device, "vkCmdTraceRaysKHR");

        e.vkGetAccelerationStructureBuildSizesKHR   = (PFN_vkGetAccelerationStructureBuildSizesKHR)  vkGetDeviceProcAddr(rtx().device, "vkGetAccelerationStructureBuildSizesKHR");
        e.vkCmdBuildAccelerationStructuresKHR       = (PFN_vkCmdBuildAccelerationStructuresKHR)      vkGetDeviceProcAddr(rtx().device, "vkCmdBuildAccelerationStructuresKHR");
        e.vkCreateAccelerationStructureKHR          = (PFN_vkCreateAccelerationStructureKHR)         vkGetDeviceProcAddr(rtx().device, "vkCreateAccelerationStructureKHR");
        e.vkDestroyAccelerationStructureKHR         = (PFN_vkDestroyAccelerationStructureKHR)        vkGetDeviceProcAddr(rtx().device, "vkDestroyAccelerationStructureKHR");
        e.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR) vkGetDeviceProcAddr(rtx().device, "vkGetAccelerationStructureDeviceAddressKHR");

        e.vkGetBufferDeviceAddress                  = (PFN_vkGetBufferDeviceAddress)                 vkGetDeviceProcAddr(rtx().device, "vkGetBufferDeviceAddress");

        e.vkCmdBeginRendering                       = (PFN_vkCmdBeginRendering)                      vkGetDeviceProcAddr(rtx().device, "vkCmdBeginRendering");
        e.vkCmdEndRendering                         = (PFN_vkCmdEndRendering)                        vkGetDeviceProcAddr(rtx().device, "vkCmdEndRendering");

        loaded = true;
    }
    return e;
}

// ────────────────────────────────────────────────
// Queue family helper
// ────────────────────────────────────────────────
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics, present, compute, transfer;
    bool complete() const noexcept { return graphics && present && compute; }
};

inline QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surf) noexcept {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& f = families[i];
        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        if (f.queueFlags & VK_QUEUE_COMPUTE_BIT)  indices.compute  = i;
        if (surf) {
            VkBool32 supp = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surf, &supp);
            if (supp) indices.present = i;
        }
        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.transfer = i;
    }

    if (!indices.compute.has_value()) indices.compute = indices.graphics;
    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;

    return indices;
}

// ────────────────────────────────────────────────
// Vulkan Instance Creation
// ────────────────────────────────────────────────
inline VkInstance createVulkanInstance() noexcept {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "AMOURANTHRTX";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 6, 0);
    appInfo.pEngineName        = "AMOURANTHRTX";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 6, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    uint32_t sdlCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);

    std::vector<const char*> extensions(sdlExts, sdlExts + sdlCount);

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkInstance inst = VK_NULL_HANDLE;
    vkCreateInstance(&ci, nullptr, &inst);
    return inst;
}

// ────────────────────────────────────────────────
// Logical Device & GPU Selection
// ────────────────────────────────────────────────
inline VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance instance,
    VkSurfaceKHR surface,
    uint32_t* out_graphics_family  = nullptr,
    uint32_t* out_present_family   = nullptr,
    uint32_t* out_compute_family   = nullptr,
    uint32_t* out_transfer_family  = nullptr
) noexcept {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) return VK_NULL_HANDLE;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices best;
    int bestScore = -1;

    for (auto pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        auto indices = findQueueFamilies(pd, surface);
        if (!indices.complete()) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());

        bool hasAll = true;
        for (const char* req : requiredDeviceExtensions) {
            if (std::none_of(exts.begin(), exts.end(),
                [req](const auto& e) { return std::strcmp(e.extensionName, req) == 0; })) {
                hasAll = false;
                break;
            }
        }
        if (!hasAll) continue;

        int score = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 1000 : 100;
        if (score > bestScore) {
            bestScore = score;
            selected = pd;
            best = indices;
        }
    }

    if (!selected) return VK_NULL_HANDLE;

    rtx().physical = selected;

    std::set<uint32_t> families = {
        best.graphics.value(),
        best.present.value(),
        best.compute.value_or(best.graphics.value())
    };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t fam : families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};
    features.shaderFloat64 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
    accel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel.accelerationStructure = VK_TRUE;
    accel.pNext = &vk12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipe{};
    rtPipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipe.rayTracingPipeline = VK_TRUE;
    rtPipe.pNext = &accel;

    std::vector<const char*> exts(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &rtPipe;
    dci.pQueueCreateInfos       = qcis.data();
    dci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    dci.pEnabledFeatures        = &features;
    dci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    dci.ppEnabledExtensionNames = exts.data();

    VkDevice dev = VK_NULL_HANDLE;
    vkCreateDevice(selected, &dci, nullptr, &dev);

    rtx().device = dev;

    rtx().graphics_family = best.graphics.value();
    rtx().present_family  = best.present.value();
    rtx().compute_family  = best.compute.value_or(best.graphics.value());
    rtx().transfer_family = best.transfer.value_or(best.graphics.value());

    vkGetDeviceQueue(dev, rtx().graphics_family, 0, &rtx().graphics_queue);
    vkGetDeviceQueue(dev, rtx().present_family,  0, &rtx().present_queue);
    vkGetDeviceQueue(dev, rtx().compute_family,  0, &rtx().compute_queue);
    vkGetDeviceQueue(dev, rtx().transfer_family, 0, &rtx().transfer_queue);

    if (out_graphics_family)  *out_graphics_family  = rtx().graphics_family;
    if (out_present_family)   *out_present_family   = rtx().present_family;
    if (out_compute_family)   *out_compute_family   = rtx().compute_family;
    if (out_transfer_family)  *out_transfer_family  = rtx().transfer_family;

    return dev;
}

// ────────────────────────────────────────────────
// Command buffer helpers (used by Memory::uploadToBuffer)
// ────────────────────────────────────────────────
inline VkCommandBuffer beginTransientCommandBuffer() noexcept {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool        = rtx().transient_pool;
    alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(rtx().device, &alloc, &cmd) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
        return VK_NULL_HANDLE;
    }

    return cmd;
}

inline void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
    if (!cmd) return;

    vkEndCommandBuffer(cmd);

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(rtx().device, &fci, nullptr, &fence);

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;

    vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
    vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(rtx().device, fence, nullptr);

    vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
}

// ────────────────────────────────────────────────
// Memory implementation
// ────────────────────────────────────────────────
namespace Memory {

enum class MemoryHint {
    Auto,           // Device local preferred
    HostVisible,    // For frequent CPU writes (staging buffers, etc.)
    HostCoherent,   // Same as HostVisible but guarantees coherence
    DeviceLocal     // Explicit device-only
};

inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags required) noexcept {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(rtx().physical, &props);

    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (props.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    return ~0u;
}

inline uint64_t createBuffer(
    VkDeviceSize        size,
    VkBufferUsageFlags  usage,
    std::string_view    tag  = "",
    MemoryHint          hint = MemoryHint::Auto
) noexcept 
{
    if (size == 0 || !rtx().device) return 0;

    // Create the buffer with shader device address support (required for SBT, AS, etc.)
    VkBufferCreateInfo bci{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(rtx().device, &bci, nullptr, &buf) != VK_SUCCESS) {
        return 0;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, buf, &req);

    // Choose memory properties based on hint
    VkMemoryPropertyFlags desiredProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (hint == MemoryHint::HostVisible || hint == MemoryHint::HostCoherent) {
        desiredProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    // You can add more cases later (e.g. HostCached)

    uint32_t memType = findMemoryType(req.memoryTypeBits, desiredProps);

    // Fallback for SBT / performance-critical buffers: prefer DEVICE_LOCAL even if host-visible was requested
    if (memType == ~0u && (hint == MemoryHint::HostVisible || hint == MemoryHint::HostCoherent)) {
        memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    if (memType == ~0u) {
        vkDestroyBuffer(rtx().device, buf, nullptr);
        return 0;
    }

    // Required for shader device address (SBT, ray tracing, etc.)
    VkMemoryAllocateFlagsInfo allocFlags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &allocFlags,
        .allocationSize  = req.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(rtx().device, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(rtx().device, buf, nullptr);
        return 0;
    }

    if (vkBindBufferMemory(rtx().device, buf, mem, 0) != VK_SUCCESS) {
        vkFreeMemory(rtx().device, mem, nullptr);
        vkDestroyBuffer(rtx().device, buf, nullptr);
        return 0;
    }

    // Get device address (needed for SBT regions)
    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buf
    };
    VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(rtx().device, &addrInfo);

    // Map only when explicitly requested as host-visible
    void* mapped = nullptr;
    bool shouldMap = (hint == MemoryHint::HostVisible || hint == MemoryHint::HostCoherent);

    if (shouldMap) {
        vkMapMemory(rtx().device, mem, 0, VK_WHOLE_SIZE, 0, &mapped);
        // Note: we ignore map failure here (rare); mapped remains nullptr
    }

    // Store in your registry
    uint64_t handle = rtx().next_buffer_handle++;
    rtx().buffers.emplace(handle, RTX::BufferInfo{
        .buffer   = buf,
        .memory   = mem,
        .size     = size,
        .deviceAddress = addr,
        .mapped   = mapped,
        .usage    = usage,
        .tag      = std::string(tag)
    });

    return handle;
}

inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    auto it = rtx().buffers.find(handle);
    if (it == rtx().buffers.end()) return;

    auto& b = it->second;
    if (b.mapped) vkUnmapMemory(rtx().device, b.memory);
    vkDestroyBuffer(rtx().device, b.buffer, nullptr);
    vkFreeMemory(rtx().device, b.memory, nullptr);
    rtx().buffers.erase(it);
}

inline RTX::BufferInfo* get(uint64_t handle) noexcept {
    auto it = rtx().buffers.find(handle);
    return (it != rtx().buffers.end()) ? &it->second : nullptr;
}

inline VkBuffer getBuffer(uint64_t handle) noexcept {
    auto* info = get(handle);
    return info ? info->buffer : VK_NULL_HANDLE;
}

inline std::pair<VkBuffer, VkDeviceMemory> uploadToBuffer(
    uint64_t            handle,
    const void*         data,
    VkDeviceSize        size,
    VkCommandBuffer     cmd = VK_NULL_HANDLE
) noexcept {
    auto* info = get(handle);
    if (!info || size > info->size) return {VK_NULL_HANDLE, VK_NULL_HANDLE};

    if (info->mapped) {
        std::memcpy(info->mapped, data, size);
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    VkBufferCreateInfo stagingCI{};
    stagingCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingCI.size  = size;
    stagingCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkBuffer staging = VK_NULL_HANDLE;
    vkCreateBuffer(rtx().device, &stagingCI, nullptr, &staging);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, staging, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    vkAllocateMemory(rtx().device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(rtx().device, staging, stagingMem, 0);

    void* ptr = nullptr;
    vkMapMemory(rtx().device, stagingMem, 0, size, 0, &ptr);
    std::memcpy(ptr, data, size);
    vkUnmapMemory(rtx().device, stagingMem);

    bool ownCmd = !cmd;
    VkCommandBuffer targetCmd = cmd ? cmd : beginTransientCommandBuffer();

    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(targetCmd, staging, info->buffer, 1, &copy);

    if (ownCmd) {
        endSubmitAndWait(targetCmd);
    }

    return {staging, stagingMem};
}

inline VRAMReality measureReality() noexcept {
    VRAMReality r{};
    VkPhysicalDeviceMemoryProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    props2.pNext = &budget;

    vkGetPhysicalDeviceMemoryProperties2(rtx().physical, &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            r.total += props2.memoryProperties.memoryHeaps[i].size;
            r.driver_footprint += budget.heapUsage[i];
        }
    }

    r.usable = r.total > (r.driver_footprint + r.safety_margin) ?
               r.total - r.driver_footprint - r.safety_margin : 0;
    r.remaining = r.usable;
    return r;
}

} // namespace Memory

// ────────────────────────────────────────────────
// Swapchain — functions in dependency order: cleanup → recreate → create
// ────────────────────────────────────────────────

namespace Swapchain {

struct Handle {
    VkSwapchainKHR value = VK_NULL_HANDLE;
    bool valid() const noexcept { return value != VK_NULL_HANDLE; }
    operator VkSwapchainKHR() const noexcept { return value; }
};

inline Handle               swapchain;
inline std::vector<VkImage> images;
inline std::vector<VkImageView> views;
inline VkExtent2D           extent{};
inline VkFormat             format          = VK_FORMAT_B8G8R8A8_SRGB;
inline VkPresentModeKHR     presentMode     = VK_PRESENT_MODE_FIFO_KHR;
inline bool                 minimized       = false;
inline bool                 needsRecreate   = false;

inline double               lastPresentTime_s   = 0.0;
inline double               smoothedRefresh_s   = 1.0 / 60.0;

// ────────────────────────────────────────────────
// Forward declarations inside namespace
// ────────────────────────────────────────────────

inline VkSwapchainKHR get() noexcept;
inline VkExtent2D     getExtent() noexcept;
inline void           updateRefreshEstimate(double t) noexcept;
inline double         getSmoothedRefresh() noexcept;

// ────────────────────────────────────────────────
// Cleanup (first — used by recreate)
// ────────────────────────────────────────────────

inline void cleanup() noexcept {
    for (auto v : views) vkDestroyImageView(rtx().device, v, nullptr);
    views.clear();
    images.clear();

    if (swapchain.valid()) {
        vkDestroySwapchainKHR(rtx().device, swapchain, nullptr);
        swapchain.value = VK_NULL_HANDLE;
    }
}

// ────────────────────────────────────────────────
// Recreate
// ────────────────────────────────────────────────

inline void recreate(int w, int h) noexcept {
    vkDeviceWaitIdle(rtx().device);

    cleanup();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rtx().physical, rtx().surface, &caps);

    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        minimized = true;
        return;
    }

    extent = (caps.currentExtent.width != UINT32_MAX) ?
             caps.currentExtent :
             VkExtent2D{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(rtx().physical, rtx().surface, &formatCount, formats.data());

    format = VK_FORMAT_B8G8R8A8_SRGB;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = f.format;
            break;
        }
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(rtx().physical, rtx().surface, &modeCount, modes.data());

    presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = rtx().surface;
    ci.minImageCount    = 2;
    ci.imageFormat      = format;
    ci.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode;
    ci.clipped          = VK_TRUE;

    vkCreateSwapchainKHR(rtx().device, &ci, nullptr, &swapchain.value);

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(rtx().device, swapchain, &imgCount, nullptr);
    images.resize(imgCount);
    vkGetSwapchainImagesKHR(rtx().device, swapchain, &imgCount, images.data());

    views.resize(imgCount);
    for (size_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = images[i];
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(rtx().device, &vi, nullptr, &views[i]);
    }

    minimized = false;
}

// ────────────────────────────────────────────────
// Create
// ────────────────────────────────────────────────

inline void create(SDL_Window* window, int w, int h) noexcept {
    rtx().window = window;
    recreate(w, h);
}

// ────────────────────────────────────────────────
// Inline helpers
// ────────────────────────────────────────────────

inline VkSwapchainKHR get() noexcept                { return swapchain; }
inline VkExtent2D     getExtent() noexcept          { return extent; }

inline void updateRefreshEstimate(double t) noexcept {
    if (lastPresentTime_s > 0.0) {
        double dt = t - lastPresentTime_s;
        if (dt > 0.0005 && dt < 0.2) smoothedRefresh_s = 0.25 * dt + 0.75 * smoothedRefresh_s;
    }
    lastPresentTime_s = t;
}

inline double getSmoothedRefresh() noexcept { return smoothedRefresh_s; }

} // namespace Swapchain

// ────────────────────────────────────────────────
// Engine init / shutdown
// ────────────────────────────────────────────────

inline bool initRTX(SDL_Window* window, int width, int height) noexcept {
    rtx().instance = createVulkanInstance();
    if (!rtx().instance) return false;

    if (SDL_Vulkan_CreateSurface(window, rtx().instance, nullptr, &rtx().surface) == 0) {
        return false;
    }

    createLogicalDeviceAndSelectGPU(rtx().instance, rtx().surface);

    if (!rtx().device) return false;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;

    vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);

    Swapchain::create(window, width, height);

    return true;
}

inline void cleanupRTX() noexcept {
    vkDeviceWaitIdle(rtx().device);

    Swapchain::cleanup();

    for (auto& [h, b] : rtx().buffers) {
        if (b.mapped) vkUnmapMemory(rtx().device, b.memory);
        vkDestroyBuffer(rtx().device, b.buffer, nullptr);
        vkFreeMemory(rtx().device, b.memory, nullptr);
    }
    rtx().buffers.clear();

    if (rtx().transient_pool) vkDestroyCommandPool(rtx().device, rtx().transient_pool, nullptr);

    if (rtx().device) vkDestroyDevice(rtx().device, nullptr);
    if (rtx().surface) vkDestroySurfaceKHR(rtx().instance, rtx().surface, nullptr);
    if (rtx().instance) vkDestroyInstance(rtx().instance, nullptr);
}