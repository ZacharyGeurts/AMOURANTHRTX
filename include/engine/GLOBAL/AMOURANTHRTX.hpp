#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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

#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include "engine/GLOBAL/ELLIE.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#define VK_KHR_acceleration_structure 1
#define VK_KHR_ray_tracing_pipeline 1
#define VK_KHR_deferred_host_operations 1
#define VK_KHR_buffer_device_address 1
#define VK_EXT_descriptor_buffer 1

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

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics, present, compute, transfer;
    [[nodiscard]] bool complete() const noexcept { return graphics && present && compute; }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) noexcept {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& f = families[i];
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
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("VULKAN", "vkCreateInstance failed: {}", string_VkResult(res));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("VULKAN", "Instance created — {} extensions, validation {}",
                    extensions.size(), Options::Debug::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF");
    return inst;
}

struct UniversalPrimitive {
    glm::vec4 aabbMin;
    glm::vec4 aabbMax;
    glm::mat4 transform;
    uint32_t type          = 0;
    uint32_t materialIndex = 0;
    float destruction      = 0.0f;
};

struct InternalMesh {
    uint64_t vertexBuffer   = 0;
    uint64_t indexBuffer    = 0;
    uint32_t primitiveCount = 0;
    uint32_t vertexCount    = 0;
    uint32_t materialIndex  = 0;
    glm::mat4 transform     = glm::mat4(1.0f);
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    uint64_t blasStorage    = 0;
    bool blasBuilt          = false;
};

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

struct StagingRing {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   size   = 1ULL << 30;
    void*          mapped = nullptr;
    VkDeviceSize   head   = 0;
    bool           ready  = false;
    VkDeviceAddress baseAddr = 0;
};

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

    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkExtent2D extent{};
    uint32_t image_count = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
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
    VkAccelerationStructureKHR las_procedural_blas = VK_NULL_HANDLE;
    uint64_t las_procedural_blas_storage = 0;

    std::vector<InternalMesh> las_triangle_meshes;
    std::vector<UniversalPrimitive> las_procedural_primitives;

    bool las_initialized = false;
    bool las_tlas_dirty = true;
    bool las_pending_blas_builds = true;
    bool las_procedural_dirty = true;

    VkBuffer mesh_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory mesh_vertex_memory = VK_NULL_HANDLE;
    VkBuffer mesh_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory mesh_index_memory = VK_NULL_HANDLE;
    uint32_t mesh_index_count = 0;

    std::vector<Chunk> buffer_chunks;
    StagingRing staging_ring{};
    std::unordered_map<uint64_t, BufferInfo> buffers;
    uint64_t next_buffer_handle = 0x00000001ULL;
    std::mutex buffer_mutex;
};

inline RTX& rtx() noexcept {
    static RTX e;
    return e;
}

[[nodiscard]] inline VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance inst,
    VkSurfaceKHR surf,
    uint32_t* out_graphics_family = nullptr,
    uint32_t* out_present_family  = nullptr,
    uint32_t* out_compute_family  = nullptr,
    uint32_t* out_transfer_family = nullptr
) noexcept {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(inst, &count, nullptr);
    if (count == 0) {
        LOG_FATAL_CAT("VULKAN", "No Vulkan GPUs found");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(inst, &count, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices best;
    int bestScore = -1;

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

        bool hasAll = true;
        for (const char* need : requiredDeviceExtensions) {
            bool found = false;
            for (const auto& e : exts) {
                if (std::strcmp(e.extensionName, need) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                hasAll = false;
                break;
            }
        }
        if (!hasAll) continue;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (std::strstr(props.deviceName, "RTX") || std::strstr(props.deviceName, "GeForce")) score += 300000;

        if (score > bestScore) {
            bestScore = score;
            selected = pd;
            best = indices;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "No RTX-capable GPU with compute support found");
        return VK_NULL_HANDLE;
    }

    // ────────────────────────────────────────────────
    // SEAL THE PHYSICAL DEVICE AS SOON AS WE HAVE IT
    // ────────────────────────────────────────────────
    rtx().physical = selected;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(selected, &props);
    LOG_INFO_CAT("VULKAN", "Selected GPU: {}", props.deviceName);

    std::set<uint32_t> uniqueQ = {best.graphics.value(), best.present.value(), best.compute.value()};
    if (best.transfer.has_value()) uniqueQ.insert(best.transfer.value());

    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    for (uint32_t fam : uniqueQ) {
        qInfos.push_back({
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = fam,
            .queueCount       = 1,
            .pQueuePriorities = &prio
        });
    }

    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress     = VK_TRUE;
    vk12.descriptorIndexing      = VK_TRUE;
    vk12.runtimeDescriptorArray  = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
    accel.sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel.accelerationStructure  = VK_TRUE;
    accel.pNext                  = &vk12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipe{};
    rtPipe.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipe.rayTracingPipeline    = VK_TRUE;
    rtPipe.pNext                 = &accel;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufFeatures{};
    descBufFeatures.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descBufFeatures.descriptorBuffer = VK_TRUE;
    descBufFeatures.pNext        = &rtPipe;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.pNext                   = &descBufFeatures;
    devInfo.queueCreateInfoCount    = static_cast<uint32_t>(qInfos.size());
    devInfo.pQueueCreateInfos       = qInfos.data();
    devInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size());
    devInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    VkDevice dev = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(selected, &devInfo, nullptr, &dev);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("VULKAN", "vkCreateDevice failed: {}", string_VkResult(res));
        // Optional: clean up the premature seal if creation fails
        rtx().physical = VK_NULL_HANDLE;
        return VK_NULL_HANDLE;
    }

    if (out_graphics_family) *out_graphics_family = best.graphics.value();
    if (out_present_family)  *out_present_family  = best.present.value();
    if (out_compute_family)  *out_compute_family  = best.compute.value();
    if (out_transfer_family) *out_transfer_family = best.transfer.value_or(best.graphics.value());

    LOG_SUCCESS_CAT("VULKAN", "Logical device created — RTX + compute + descriptor buffer enabled");

    return dev;
}

[[noreturn]] inline void issue_execution_order(
    std::string_view category,
    std::string_view expected,
    std::string_view actual,
    const std::source_location& loc = std::source_location::current()) noexcept {
    time_t now = time(nullptr);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC", gmtime(&now));

    std::string buf = std::format(
        "\n╔════════════════════════════════════════════════════════════════════════════╗\n"
        "║                       EXECUTION ORDER ISSUED                               ║\n"
        "╟────────────────────────────────────────────────────────────────────────────╢\n"
        "║ Timestamp: {}\n"
        "║ Category:  {}\n"
        "║ Location:  {}:{} ({})\n"
        "║ Expected:  {}\n"
        "║ Actual:    {}\n"
        "RTX compromised — immediate termination.\n",
        timestamp,
        category,
        loc.file_name(), loc.line(), loc.function_name(),
        expected,
        actual
    );

    auto st = std::stacktrace::current();
    buf += "Backtrace:\n";
    for (size_t i = 0; i < st.size(); ++i) {
        buf += std::format("  [{}] {}\n", i, st[i].description());
    }

    LOG_FATAL_CAT("RTX", "{}", buf);
    std::abort();
}

#define SEAL_CHECK(expected, actual, category) \
    do { \
        if ((expected) != (actual)) { \
            issue_execution_order(category, std::string_view(#expected), std::string_view(#actual)); \
        } \
    } while(0)

inline void stone_seal_device_resources(VkInstance i, VkDevice d, VkPhysicalDevice p,
                                        VkSurfaceKHR s, VkSwapchainKHR sc) noexcept {
    rtx().instance = i;
    rtx().device = d;
    rtx().physical = p;
    rtx().surface = s;
    rtx().swapchain = sc;
}

inline void stone_seal_queues(VkQueue graphics, VkQueue present, VkQueue compute, VkQueue transfer) noexcept {
    rtx().graphics_queue = graphics;
    rtx().present_queue = present;
    rtx().compute_queue = compute;
    rtx().transfer_queue = transfer;
}

inline void stone_seal_families(uint32_t graphics, uint32_t present, uint32_t transfer, uint32_t compute) noexcept {
    rtx().graphics_family = graphics;
    rtx().present_family = present;
    rtx().transfer_family = transfer;
    rtx().compute_family = compute;
}

inline void stone_seal_pipelines(VkPipeline compute, VkPipeline rt, VkPipelineLayout layout) noexcept {
    rtx().compute_pipeline = compute;
    rtx().rt_pipeline = rt;
    rtx().pipeline_layout = layout;
}

inline void stone_seal_window_and_pass(SDL_Window* window, VkRenderPass pass) noexcept {
    rtx().window = window;
    rtx().pass = pass;
}

inline void stone_seal_swapchain_resources(const std::vector<VkImage>& images,
                                           const std::vector<VkImageView>& views,
                                           VkExtent2D extent,
                                           uint32_t image_count) noexcept {
    rtx().images = images;
    rtx().views = views;
    rtx().extent = extent;
    rtx().image_count = image_count;
}

inline void stone_seal_rtprops(const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props) noexcept {
    rtx().rt_props = props;
}

inline void stone_seal_mesh_buffers(VkBuffer vb, VkDeviceMemory vm, VkBuffer ib, VkDeviceMemory im, uint32_t ic) noexcept {
    rtx().mesh_vertex_buffer = vb;
    rtx().mesh_vertex_memory = vm;
    rtx().mesh_index_buffer = ib;
    rtx().mesh_index_memory = im;
    rtx().mesh_index_count = ic;
}

inline void stone_seal_transient_pool(VkCommandPool pool) noexcept {
    rtx().transient_pool = pool;
}

inline void stone_seal_final() noexcept {
    LOG_AMOURANTH("AMOURANTHRTX v0.81 — FINAL RTX SEAL FORGED — FULL ACCESS GRANTED — ALL RESOURCES LOCKED");
}

// Stone accessors
inline VkDevice stone_device() noexcept { return rtx().device; }
inline VkPhysicalDevice stone_physical() noexcept { return rtx().physical; }
inline VkSurfaceKHR stone_surface() noexcept { return rtx().surface; }
inline VkSwapchainKHR stone_swapchain() noexcept { return rtx().swapchain; }
inline VkQueue stone_graphics_queue() noexcept { return rtx().graphics_queue; }

// LAS accessors
inline auto& las_procedural_primitives() noexcept { return rtx().las_procedural_primitives; }
inline bool& las_procedural_dirty() noexcept { return rtx().las_procedural_dirty; }
inline bool& las_tlas_dirty() noexcept { return rtx().las_tlas_dirty; }
inline bool& las_pending_blas_builds() noexcept { return rtx().las_pending_blas_builds; }
inline bool& las_initialized() noexcept { return rtx().las_initialized; }
inline VkAccelerationStructureKHR stone_las_tlas() noexcept { return rtx().las_tlas; }

// VulkanExtensions — singleton
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

    static bool instanceLoaded = false;
    static bool deviceLoaded = false;

    if (!instanceLoaded) {
        VkInstance inst = rtx().instance;
        if (inst) {
            auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                SDL_Vulkan_GetVkGetInstanceProcAddr());

            if (vkGetInstanceProcAddr) {
#define LOAD_INSTANCE(fn) \
                e.fn = reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(inst, #fn))

                LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
                LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
                LOAD_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
                LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

#undef LOAD_INSTANCE

                instanceLoaded = true;
            }
        }
    }

    if (!deviceLoaded) {
        VkDevice dev = rtx().device;
        if (dev) {
            auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                SDL_Vulkan_GetVkGetInstanceProcAddr());

            if (vkGetInstanceProcAddr) {
                auto vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    vkGetInstanceProcAddr(rtx().instance, "vkGetDeviceProcAddr"));

                if (vkGetDeviceProcAddr) {
#define LOAD(fn) \
                    e.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(dev, #fn)); \
                    if (!e.fn) { \
                        LOG_INFO_CAT("VULKAN", "Optional extension unavailable: {}", #fn); \
                    }

                    LOAD(vkCreateSwapchainKHR);
                    LOAD(vkDestroySwapchainKHR);
                    LOAD(vkGetSwapchainImagesKHR);
                    LOAD(vkAcquireNextImageKHR);
                    LOAD(vkQueuePresentKHR);

                    LOAD(vkCreateRayTracingPipelinesKHR);
                    LOAD(vkGetRayTracingShaderGroupHandlesKHR);
                    LOAD(vkCmdTraceRaysKHR);

                    LOAD(vkGetAccelerationStructureBuildSizesKHR);
                    LOAD(vkCmdBuildAccelerationStructuresKHR);
                    LOAD(vkCreateAccelerationStructureKHR);
                    LOAD(vkDestroyAccelerationStructureKHR);
                    LOAD(vkGetAccelerationStructureDeviceAddressKHR);

                    LOAD(vkGetBufferDeviceAddress);

                    LOAD(vkCmdCopyAccelerationStructureKHR);
                    LOAD(vkCmdWriteAccelerationStructuresPropertiesKHR);
                    LOAD(vkCmdTraceRaysIndirect2KHR);

                    LOAD(vkCmdBeginRendering);
                    LOAD(vkCmdEndRendering);
                    LOAD(vkCmdPipelineBarrier2);
                    LOAD(vkQueueSubmit2KHR);

                    LOAD(vkSetDebugUtilsObjectNameEXT);

                    LOAD(vkGetDescriptorSetLayoutSizeEXT);
                    LOAD(vkGetDescriptorSetLayoutBindingOffsetEXT);
                    LOAD(vkCmdBindDescriptorBuffersEXT);
                    LOAD(vkCmdSetDescriptorBufferOffsetsEXT);
                    LOAD(vkGetDescriptorEXT);

#undef LOAD

                    deviceLoaded = true;
                }
            }
        }
    }

    return e;
}

// MACROS — LEAN AND CENTRALIZED
#define VK_CREATE_RT_PIPELINES(...)              ext().vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             ext().vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              ext().vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_CMD_TRACE_RAYS_INDIRECT2(cmd, ...)    ext().vkCmdTraceRaysIndirect2KHR(cmd, __VA_ARGS__)

#define VK_GET_AS_BUILD_SIZES(...)               ext().vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) ext().vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    ext().vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   ext().vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            ext().vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

#define VK_GET_BUFFER_DEVICE_ADDRESS(...)        ext().vkGetBufferDeviceAddress(__VA_ARGS__)

#define VK_CMD_COPY_ACCELERATION_STRUCTURE(cmd, info) \
    ext().vkCmdCopyAccelerationStructureKHR(cmd, info)

#define VK_CMD_WRITE_AS_PROPERTIES(cmd, count, as, queryType, queryPool, query) \
    ext().vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, count, as, queryType, queryPool, query)

#define VK_CREATE_SWAPCHAIN(...)                 ext().vkCreateSwapchainKHR(__VA_ARGS__)
#define VK_DESTROY_SWAPCHAIN(...)                ext().vkDestroySwapchainKHR(__VA_ARGS__)
#define VK_GET_SWAPCHAIN_IMAGES(...)             ext().vkGetSwapchainImagesKHR(__VA_ARGS__)
#define VK_ACQUIRE_NEXT_IMAGE(...)               ext().vkAcquireNextImageKHR(__VA_ARGS__)
#define VK_QUEUE_PRESENT(...)                    ext().vkQueuePresentKHR(__VA_ARGS__)

#define VK_GET_DESCRIPTOR_SET_LAYOUT_SIZE(layout, size) \
    ext().vkGetDescriptorSetLayoutSizeEXT(rtx().device, layout, size)

#define VK_GET_DESCRIPTOR_BINDING_OFFSET(layout, binding, offset) \
    ext().vkGetDescriptorSetLayoutBindingOffsetEXT(rtx().device, layout, binding, offset)

#define VK_CMD_BIND_DESCRIPTOR_BUFFERS(cmd, count, bindingInfos) \
    ext().vkCmdBindDescriptorBuffersEXT(cmd, count, bindingInfos)

#define VK_CMD_SET_DESCRIPTOR_BUFFER_OFFSETS(cmd, bindPoint, layout, firstSet, count, indices, offsets) \
    ext().vkCmdSetDescriptorBufferOffsetsEXT(cmd, bindPoint, layout, firstSet, count, indices, offsets)

#define VK_GET_DESCRIPTOR(device, getInfo, size, outDescriptor) \
    ext().vkGetDescriptorEXT(device, getInfo, size, outDescriptor)

// BufferManager helpers
inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE     = 256ULL << 20;
inline constexpr VkDeviceSize TINY_SAFETY_MARGIN     = 256ULL << 20;
inline constexpr VkDeviceSize STAGING_RING_SIZE      = 1ULL << 30;

inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL << 10;
inline constexpr VkDeviceSize SBT_MINIMUM_SIZE       = 512;
inline constexpr VkDeviceSize SBT_ALIGNMENT          = 256;

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
    return ~0u;
}

struct VRAMReality {
    VkDeviceSize total            = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin    = TINY_SAFETY_MARGIN;
    VkDeviceSize usable           = 0;
    VkDeviceSize remaining        = 0;
};

[[nodiscard]] inline VRAMReality measureReality() noexcept {
    static bool measured = false;
    static VRAMReality reality{};

    if (measured) return reality;

    VkPhysicalDevice phys = rtx().physical;

    VkPhysicalDeviceMemoryProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    vkGetPhysicalDeviceMemoryProperties2(phys, &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.total += props2.memoryProperties.memoryHeaps[i].size;
        }
    }

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    props2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(phys, &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.driver_footprint = budget.heapUsage[i];
            break;
        }
    }

    if (reality.driver_footprint == 0) {
        reality.driver_footprint = 1'500'000'000ULL;
    }

    reality.usable = reality.total > (reality.driver_footprint + reality.safety_margin)
                   ? reality.total - reality.driver_footprint - reality.safety_margin
                   : 0;

    reality.remaining = reality.usable;

    measured = true;
    return reality;
}

[[nodiscard]] inline VkDeviceSize availableToTake() noexcept {
    return measureReality().remaining;
}

inline void ensureStagingRing() noexcept {
    auto& ring = rtx().staging_ring;
    if (ring.ready) return;

    VkDevice dev = rtx().device;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = STAGING_RING_SIZE;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(dev, &bci, nullptr, &ring.buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, ring.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) return;

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    vkAllocateMemory(dev, &mai, nullptr, &ring.memory);
    vkBindBufferMemory(dev, ring.buffer, ring.memory, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = ring.buffer;
    ring.baseAddr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    vkMapMemory(dev, ring.memory, 0, VK_WHOLE_SIZE, 0, &ring.mapped);

    ring.ready = true;
}

[[nodiscard]] inline void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();
    auto& ring = rtx().staging_ring;
    VkDeviceSize offset = ring.head;
    ring.head = (ring.head + align_up(size, 256)) % ring.size;
    return static_cast<std::byte*>(ring.mapped) + offset;
}

[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return rtx().staging_ring.buffer;
}

[[nodiscard]] inline Chunk* createChunk(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE) noexcept {
    VkDevice dev = rtx().device;

    VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, size);

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = chunkSize;
    bci.usage = CHUNK_USAGE_FLAGS | usage;
    bci.sharingMode = sharingMode;

    VkBuffer buffer = VK_NULL_HANDLE;
    vkCreateBuffer(dev, &bci, nullptr, &buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return nullptr;
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
    vkAllocateMemory(dev, &mai, nullptr, &memory);
    vkBindBufferMemory(dev, buffer, memory, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress baseAddr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    auto& chunks = rtx().buffer_chunks;
    chunks.push_back({buffer, memory, chunkSize, baseAddr, 0, "Chunk_" + std::to_string(chunks.size()), {}});

    return &chunks.back();
}

[[nodiscard]] inline uint64_t createDescriptorBuffer(VkDeviceSize size, std::string_view tag = "EternalDescriptorBuffer") noexcept {
    if (size == 0) size = 4096ULL;

    VkDevice dev = rtx().device;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    vkCreateBuffer(dev, &bci, nullptr, &buffer);

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
    vkAllocateMemory(dev, &mai, nullptr, &memory);
    vkBindBufferMemory(dev, buffer, memory, 0);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress addr = ext().vkGetBufferDeviceAddress(dev, &addrInfo);

    auto& buffers = rtx().buffers;
    uint64_t handle = ++rtx().next_buffer_handle;
    buffers.emplace(handle, BufferInfo{
        buffer, memory, size, req.alignment, 0, addr,
        nullptr, usage, std::string(tag)
    });

    return handle;
}

[[nodiscard]] inline void* lazyMapDescriptor(uint64_t handle) noexcept {
    auto& buffers = rtx().buffers;
    auto it = buffers.find(handle);
    if (it == buffers.end()) return nullptr;

    BufferInfo& info = it->second;
    if ((info.usage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) == 0) return nullptr;

    if (info.mapped != nullptr) return info.mapped;

    VkDevice dev = rtx().device;

    void* mapped = nullptr;
    vkMapMemory(dev, info.memory, 0, info.size, 0, &mapped);

    info.mapped = mapped;
    return mapped;
}

[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage,
                                     std::string_view tag = "") noexcept {
    if (size == 0) return 0;

    VkBufferUsageFlags fixedUsage = usage;

    if (fixedUsage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)) {
        fixedUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    if (fixedUsage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) {
        return createDescriptorBuffer(size, tag);
    }

    bool isPureStaging = (usage == VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!isPureStaging) fixedUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    bool isSBT = (fixedUsage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) ||
                 (tag.find("SBT") != std::string_view::npos);

    if (isSBT) {
        size = std::max(size, SBT_MINIMUM_SIZE);
        size = align_up(size, SBT_ALIGNMENT);
        fixedUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bool smallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                        (fixedUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    if (smallUniform) {
        ensureStagingRing();

        auto& ring = rtx().staging_ring;
        VkDeviceSize alignedSize = align_up(size, 256);
        VkDeviceSize offset = ring.head;
        ring.head = (ring.head + alignedSize) % ring.size;

        uint64_t handle = ++rtx().next_buffer_handle;
        rtx().buffers.emplace(handle, BufferInfo{
            ring.buffer, ring.memory,
            size, alignedSize, offset,
            ring.baseAddr,
            static_cast<std::byte*>(ring.mapped) + offset,
            fixedUsage,
            std::string(tag)
        });
        return handle;
    }

    VkDeviceSize remaining = size;
    uint64_t firstHandle = 0;

    while (remaining > 0) {
        VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, remaining);
        Chunk* chunk = createChunk(chunkSize, fixedUsage);
        if (!chunk) return 0;

        uint64_t chunkHandle = ++rtx().next_buffer_handle;
        rtx().buffers.emplace(chunkHandle, BufferInfo{
            chunk->buffer, chunk->memory, chunkSize, chunk->size, chunk->head,
            chunk->baseAddr + chunk->head, nullptr, fixedUsage, std::string(tag) + "_chunk"
        });

        chunk->handles.push_back(chunkHandle);

        if (firstHandle == 0) firstHandle = chunkHandle;

        remaining -= chunkSize;
    }

    return firstHandle;
}

[[nodiscard]] inline VkDeviceAddress allocateScratch(VkDeviceSize requiredSize) noexcept {
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

        if (chunkHandle == 0) return 0;

        auto it = rtx().buffers.find(chunkHandle);
        if (it == rtx().buffers.end()) return 0;

        VkDeviceAddress chunkAddr = it->second.deviceAddress + it->second.offset;
        if (total == 0) baseAddr = chunkAddr;

        total += chunkSize;
    }

    return baseAddr;
}

inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size,
                           VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    auto& buffers = rtx().buffers;
    auto it = buffers.find(handle);
    if (it == buffers.end() || size > it->second.size) return;

    BufferInfo& info = it->second;

    if (info.buffer == VK_NULL_HANDLE) return;

    if (info.mapped != nullptr) {
        std::memcpy(info.mapped, data, size);
        return;
    }

    VkDevice dev = rtx().device;
    VkQueue queue = rtx().graphics_queue;

    if (cmd != VK_NULL_HANDLE) {
        void* staging = mapStaging(size);
        if (!staging) return;
        std::memcpy(staging, data, size);

        VkBufferCopy copy{};
        copy.srcOffset = static_cast<VkDeviceSize>(
            reinterpret_cast<uintptr_t>(staging) -
            reinterpret_cast<uintptr_t>(rtx().staging_ring.mapped)
        );
        copy.dstOffset = info.offset;
        copy.size = size;

        vkCmdCopyBuffer(cmd, rtx().staging_ring.buffer, info.buffer, 1, &copy);
        return;
    }

    VkCommandPool transientPool = VK_NULL_HANDLE;
    VkCommandBuffer tempCmd = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = rtx().graphics_family;
    vkCreateCommandPool(dev, &pci, nullptr, &transientPool);

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = transientPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(dev, &ai, &tempCmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tempCmd, &bi);

    void* staging = mapStaging(size);
    if (!staging) {
        vkEndCommandBuffer(tempCmd);
        vkFreeCommandBuffers(dev, transientPool, 1, &tempCmd);
        vkDestroyCommandPool(dev, transientPool, nullptr);
        return;
    }
    std::memcpy(staging, data, size);

    VkBufferCopy copy{};
    copy.srcOffset = static_cast<VkDeviceSize>(
        reinterpret_cast<uintptr_t>(staging) -
        reinterpret_cast<uintptr_t>(rtx().staging_ring.mapped)
    );
    copy.dstOffset = info.offset;
    copy.size = size;

    vkCmdCopyBuffer(tempCmd, rtx().staging_ring.buffer, info.buffer, 1, &copy);

    vkEndCommandBuffer(tempCmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &tempCmd;

    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(dev, transientPool, 1, &tempCmd);
    vkDestroyCommandPool(dev, transientPool, nullptr);
}

inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(rtx().buffer_mutex);
    auto& buffers = rtx().buffers;
    auto it = buffers.find(handle);
    if (it == buffers.end()) return;

    BufferInfo& info = it->second;
    VkDevice dev = rtx().device;

    if (info.mapped != nullptr) {
        vkUnmapMemory(dev, info.memory);
    }
    vkDestroyBuffer(dev, info.buffer, nullptr);
    vkFreeMemory(dev, info.memory, nullptr);

    buffers.erase(it);
}

#define BM_CREATE(h, s, u, ...)             h = create(s, u, ##__VA_ARGS__)
#define BM_CREATE_DESCRIPTOR(h, s, ...)     h = createDescriptorBuffer(s, ##__VA_ARGS__)
#define BM_DESTROY(h)                       destroy(h)
#define BM_GET(h)                           [&](){ auto it = rtx().buffers.find(h); return (it != rtx().buffers.end()) ? &it->second : nullptr; }()
#define BM_GET_BUFFER(h)                    (BM_GET(h) ? BM_GET(h)->buffer : VK_NULL_HANDLE)
#define BM_GET_MEMORY(h)                    (BM_GET(h) ? BM_GET(h)->memory : VK_NULL_HANDLE)
#define BM_GET_DEVICE_ADDRESS(h)            (BM_GET(h) ? BM_GET(h)->deviceAddress + BM_GET(h)->offset : 0)
#define BM_UPLOAD_TO_BUFFER(h, d, sz, ...)  uploadToBuffer(h, d, sz, ##__VA_ARGS__)
#define BM_ALLOC_SCRATCH(sz)                allocateScratch(sz)
#define BM_LAZY_MAP_DESCRIPTOR(h)           lazyMapDescriptor(h)
#define BM_AVAILABLE_VRAM()                 availableToTake()

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

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

inline void writeAccelerationStructureDescriptor(
    VkDescriptorSet set,
    uint32_t binding,
    uint32_t arrayElement,
    VkAccelerationStructureKHR accel) noexcept {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &accel;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = &asWrite;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = arrayElement;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    vkUpdateDescriptorSets(rtx().device, 1, &write, 0, nullptr);
}

inline size_t addAABBFromMesh(std::unique_ptr<Mesh> mesh, uint32_t materialIndex = 0,
                              const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    if (!mesh) return las_procedural_primitives().size();

    mesh->computeAABB();

    UniversalPrimitive p{};
    p.aabbMin       = glm::vec4(mesh->aabbMin, 0.0f);
    p.aabbMax       = glm::vec4(mesh->aabbMax, 0.0f);
    p.transform     = transform;
    p.type          = 0;
    p.materialIndex = materialIndex;
    p.destruction   = 0.0f;

    las_procedural_primitives().push_back(p);
    las_procedural_dirty() = true;
    las_tlas_dirty() = true;
    las_pending_blas_builds() = true;

    LOG_SUCCESS_CAT("LAS", "Mesh converted to AABB — min: ({},{},{}) | max: ({},{},{}) | material: {}",
                    mesh->aabbMin.x, mesh->aabbMin.y, mesh->aabbMin.z,
                    mesh->aabbMax.x, mesh->aabbMax.y, mesh->aabbMax.z,
                    materialIndex);

    return las_procedural_primitives().size() - 1;
}

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

inline size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                                uint32_t materialIndex = 0,
                                const glm::mat4& transform = glm::mat4(1.0f)) noexcept {
    UniversalPrimitive p{};
    p.aabbMin       = glm::vec4(center - glm::vec3(scale), 0.0f);
    p.aabbMax       = glm::vec4(center + glm::vec3(scale), 0.0f);
    p.transform     = transform;
    p.type          = static_cast<uint32_t>(type);
    p.materialIndex = materialIndex;
    p.destruction   = 0.0f;

    las_procedural_primitives().push_back(p);
    las_procedural_dirty() = true;
    las_tlas_dirty() = true;
    las_pending_blas_builds() = true;

    LOG_INFO_CAT("LAS", "Procedural AABB added — type {}, scale {}, material {}", 
                 static_cast<int>(type), scale, materialIndex);

    return las_procedural_primitives().size() - 1;
}

[[nodiscard]] inline std::unique_ptr<Mesh> createPlane(float width = 1000.0f, float depth = 1000.0f) noexcept {
    auto mesh = std::make_unique<Mesh>();

    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;

    mesh->aabbMin = {-halfW, -0.01f, -halfD};
    mesh->aabbMax = { halfW,  0.01f,  halfD};

    LOG_SUCCESS_CAT("LAS", "Procedural plane AABB created — width: {} | depth: {}", width, depth);

    return mesh;
}

[[nodiscard]] inline std::unique_ptr<Mesh> createBillboard() noexcept {
    auto mesh = std::make_unique<Mesh>();

    mesh->aabbMin = {-0.5f, -0.5f, -0.01f};
    mesh->aabbMax = { 0.5f,  0.5f,  0.01f};

    LOG_SUCCESS_CAT("LAS", "Procedural billboard AABB created — sacred pink quad");

    return mesh;
}

[[nodiscard]] inline std::unique_ptr<Mesh> loadOBJ(std::string_view path) noexcept {
    LOG_ATTEMPT_CAT("LAS", "Loading OBJ as AABB: {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string baseDir = "assets/models/";
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, std::string(path).c_str(), baseDir.c_str())) {
        if (!err.empty())  LOG_FATAL_CAT("LAS", "{}", err);
        if (!warn.empty()) LOG_WARNING_CAT("LAS", "{}", warn);
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh>();

    glm::vec3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            if (index.vertex_index < 0) continue;

            glm::vec3 pos{
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            min = glm::min(min, pos);
            max = glm::max(max, pos);
        }
    }

    if (glm::any(glm::lessThan(max, min))) {
        LOG_ERROR_CAT("LAS", "Failed to compute valid AABB from OBJ");
        return nullptr;
    }

    mesh->aabbMin = min;
    mesh->aabbMax = max;

    glm::vec3 padding = (max - min) * 0.001f;
    mesh->aabbMin -= padding;
    mesh->aabbMax += padding;

    LOG_SUCCESS_CAT("LAS", "OBJ loaded as AABB — min: ({},{},{}) | max: ({},{},{})",
                    min.x, min.y, min.z, max.x, max.y, max.z);

    return mesh;
}

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
        float angle = i * (3.14159f * 2.0f / 6.0f);
        glm::vec3 pos(std::cos(angle) * ringRadius, 3.0f, std::sin(angle) * ringRadius);
        addProceduralAABB(GeometryType::ProceduralD6, pos, 2.0f, 5 + i,
                          glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0)));
    }

    addProceduralAABB(GeometryType::ProceduralD100, glm::vec3(0, 7, -14), 4.0f, 11,
                      glm::rotate(glm::mat4(1.0f), 0.25f, glm::vec3(0,1,0)));

    addProceduralAABB(GeometryType::ProceduralCylinder, glm::vec3(-15, 10, -15), 2.0f, 6);
    addProceduralAABB(GeometryType::ProceduralCone, glm::vec3(0, 15, 0), 5.0f, 7);

    LOG_SUCCESS_CAT("LAS", "Default hybrid AABB scene created — {} procedurals", las_procedural_primitives().size());
}

inline void onResize() noexcept {
    las_tlas_dirty() = true;
    las_pending_blas_builds() = true;
    las_procedural_dirty() = true;
    LOG_INFO_CAT("LAS", "Resize detected — marked dirty for rebuild");
}

inline void ensureReady(VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    if (las_initialized() && !las_tlas_dirty() && !las_pending_blas_builds() && 
        !las_procedural_dirty() && stone_las_tlas() != VK_NULL_HANDLE) {
        return;
    }

    if (!las_initialized()) {
        createDefaultHybridScene();
        las_initialized() = true;
    }

    if (cmd) {
        if (las_tlas_dirty()) {
            las_tlas_dirty() = false;
        }
        las_pending_blas_builds() = false;
        las_procedural_dirty() = false;
    } else {
        LOG_WARNING_CAT("LAS", "No command buffer — LAS rebuild deferred until next frame");
    }
}

[[nodiscard]] inline VkAccelerationStructureKHR getTLAS() noexcept {
    ensureReady();
    return stone_las_tlas();
}

struct Swapchain {
    struct Handle {
        VkSwapchainKHR value;
        Handle() : value(VK_NULL_HANDLE) {}
        explicit Handle(VkSwapchainKHR v) : value(v) {}
        VkSwapchainKHR get() const { return value; }
        void reset() { value = VK_NULL_HANDLE; }
    };

    inline static Handle swapchain_;
    inline static std::vector<VkImage> swapchainImages_;
    inline static std::vector<VkImageView> swapchainImageViews_;
    inline static VkExtent2D swapchainExtent_{0, 0};
    inline static VkFormat swapchainFormat_{};
    inline static bool minimized_ = false;
    inline static bool directWriteEnabled = false;

    static void ensureReady(uint32_t w, uint32_t h) noexcept {
        if (w == 0 || h == 0) {
            minimized_ = true;
            return;
        }

        vkDeviceWaitIdle(stone_device());

        createOrRecreateSwapchain(w, h, true);
    }

    static void createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate) noexcept {
        ext();  // Ensure extensions are loaded

        VkDevice dev = stone_device();
        VkPhysicalDevice phys = stone_physical();
        VkSurfaceKHR surf = stone_surface();

        if (!dev || !phys || !surf || w == 0 || h == 0) {
            minimized_ = true;
            return;
        }

        vkDeviceWaitIdle(dev);

        if (isRecreate) {
            cleanupImageViews();
            if (swapchain_.get() != VK_NULL_HANDLE) {
                VK_DESTROY_SWAPCHAIN(dev, swapchain_.get(), nullptr);
                swapchain_.reset();
            }
            onResize();
        }

        minimized_ = false;

        VkSurfaceCapabilitiesKHR caps{};
        ext().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps);

        VkExtent2D extent{};
        if (caps.currentExtent.width == UINT32_MAX) {
            extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
            extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
        } else {
            extent = caps.currentExtent;
        }

        if (extent.width == 0 || extent.height == 0) {
            minimized_ = true;
            return;
        }

        uint32_t fmtCount = 0;
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, nullptr);

        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        ext().vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, formats.data());

        VkSurfaceFormatKHR chosenFormat{};
        if (!formats.empty()) {
            chosenFormat = formats.front();

            constexpr std::array prefs = {
                VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            };

            for (const auto& pref : prefs) {
                auto it = std::find_if(formats.begin(), formats.end(),
                                       [&](const auto& f) { return f.format == pref.format && f.colorSpace == pref.colorSpace; });
                if (it != formats.end()) {
                    chosenFormat = *it;
                    break;
                }
            }
        }

        VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;

        uint32_t imgCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

        directWriteEnabled = false;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
            VkImageFormatProperties props{};
            if (vkGetPhysicalDeviceImageFormatProperties(phys, chosenFormat.format, VK_IMAGE_TYPE_2D,
                                                         VK_IMAGE_TILING_OPTIMAL,
                                                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                         0, &props) == VK_SUCCESS) {
                directWriteEnabled = true;
                usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            }
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = surf;
        ci.minImageCount    = imgCount;
        ci.imageFormat      = chosenFormat.format;
        ci.imageColorSpace  = chosenFormat.colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform     = caps.currentTransform;
        ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode      = chosenPM;
        ci.clipped          = VK_TRUE;
        ci.oldSwapchain     = swapchain_.get();
        ci.imageUsage       = usage;

        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        VK_CREATE_SWAPCHAIN(dev, &ci, nullptr, &newSwap);

        swapchain_ = Handle(newSwap);
        swapchainExtent_ = extent;
        swapchainFormat_ = chosenFormat.format;

        uint32_t count = 0;
        VK_GET_SWAPCHAIN_IMAGES(dev, newSwap, &count, nullptr);
        swapchainImages_.resize(count);
        VK_GET_SWAPCHAIN_IMAGES(dev, newSwap, &count, swapchainImages_.data());

        swapchainImageViews_.resize(count);

        VkImageViewCreateInfo viewCI{};
        viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format           = chosenFormat.format;
        viewCI.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        for (uint32_t i = 0; i < count; ++i) {
            viewCI.image = swapchainImages_[i];
            vkCreateImageView(dev, &viewCI, nullptr, &swapchainImageViews_[i]);
        }

        stone_seal_swapchain_resources(swapchainImages_, swapchainImageViews_, extent, count);
    }

    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex, VkSemaphore* pSemaphoreOut) noexcept {
        if (minimized_) return VK_NOT_READY;

        VkDevice dev = stone_device();
        VkSwapchainKHR sw = stone_swapchain();

        static VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
        if (acquireSemaphore == VK_NULL_HANDLE) {
            VkSemaphoreCreateInfo semCI{};
            semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(dev, &semCI, nullptr, &acquireSemaphore);
        }

        VkResult res = VK_ACQUIRE_NEXT_IMAGE(dev, sw, UINT64_MAX,
                                             acquireSemaphore, VK_NULL_HANDLE, pImageIndex);

        if (res != VK_SUCCESS && res != VK_NOT_READY) {
            if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
                minimized_ = true;
            }
        }

        *pSemaphoreOut = acquireSemaphore;
        return res;
    }

    static void presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore, VkSwapchainKHR swapchainHandle) noexcept {
        if (minimized_) return;

        VkDevice dev = stone_device();

        if (imageIndex >= swapchainImages_.size()) return;

        VkImage image = swapchainImages_[imageIndex];

        static VkCommandBuffer presentCmd = VK_NULL_HANDLE;
        if (presentCmd == VK_NULL_HANDLE) {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool        = rtx().transient_pool;
            allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(dev, &allocInfo, &presentCmd);
        }

        vkResetCommandBuffer(presentCmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;

        vkBeginCommandBuffer(presentCmd, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout               = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout               = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                   = image;
        barrier.subresourceRange        = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask           = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask           = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(presentCmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        vkEndCommandBuffer(presentCmd);

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &presentCmd;

        if (waitSemaphore != VK_NULL_HANDLE) {
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
            submit.waitSemaphoreCount   = 1;
            submit.pWaitSemaphores      = &waitSemaphore;
            submit.pWaitDstStageMask    = waitStages;
        }

        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);

        VkPresentInfoKHR pi{};
        pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &swapchainHandle;
        pi.pImageIndices      = &imageIndex;

        if (waitSemaphore != VK_NULL_HANDLE) {
            pi.waitSemaphoreCount = 1;
            pi.pWaitSemaphores    = &waitSemaphore;
        }

        VK_QUEUE_PRESENT(queue, &pi);
    }

    static void recreate(uint32_t width, uint32_t height) noexcept {
        vkDeviceWaitIdle(stone_device());
        createOrRecreateSwapchain(width, height, true);
    }

    static void create(SDL_Window*, uint32_t width, uint32_t height) noexcept {
        createOrRecreateSwapchain(width, height, false);
    }

    static void cleanup() noexcept {
        VkDevice dev = stone_device();
        if (!dev) return;
        vkDeviceWaitIdle(dev);

        onResize();
        cleanupImageViews();
        cleanupSwapchain();

        if (rtx().transient_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(dev, rtx().transient_pool, nullptr);
            rtx().transient_pool = VK_NULL_HANDLE;
        }

        static VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
        if (acquireSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(dev, acquireSemaphore, nullptr);
            acquireSemaphore = VK_NULL_HANDLE;
        }
    }

    static void cleanupSwapchain() noexcept {
        swapchain_.reset();
        swapchainImages_.clear();
    }

    static void cleanupImageViews() noexcept {
        VkDevice dev = stone_device();
        for (auto v : swapchainImageViews_) {
            vkDestroyImageView(dev, v, nullptr);
        }
        swapchainImageViews_.clear();
    }
};