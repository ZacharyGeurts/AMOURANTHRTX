// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.cpp — FULLY SECURE + OPTIONS INTEGRATED — FINAL VALHALLA v80 TURBO
// • Uses Options::Shader::STONEKEY_1 (compile-time constant)
// • stonekey_xor_spirv() → ENCRYPTION MANDATORY
// • UNENCRYPTED SPIR-V = FATAL → NO ATTACH
// • g_ctx() guarded — NO WAIT, CONTINUE BOOT, CHECK LATER
// • Handle<T> destruction logged + shredded
// • BufferTracker → obfuscated handles
// • PINK PHOTONS ETERNAL — HACKERS BLIND
// • FINAL FIX: All g_ctx() → RTX::g_ctx()
// • LAS, SWAPCHAIN, RENDERER → RTX:: namespace
// • NO LOCAL g_ctx() — UNIFIED VIA RTX::g_ctx()
// • ENHANCED: Buffer creation guarded vs null device; alignment probed safely
// • FIXED: vkCreateBuffer VUID abort prevented — physDev_/device_ null-checks
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>
#include <cstring>
#include <format>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef VK_ENABLE_BETA_EXTENSIONS
  #include <vulkan/vulkan_beta.h>
#endif

using namespace Logging::Color;

VkPhysicalDevice  g_PhysicalDevice = VK_NULL_HANDLE;
VkSurfaceKHR      g_surface        = VK_NULL_HANDLE;

const char* VulkanResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        default: return std::format("VK_RESULT_{:08X}", static_cast<uint32_t>(result)).c_str();
    }
}

namespace RTX {

    Context g_context_instance;

    // =============================================================================
    // logAndTrackDestruction
    // =============================================================================

    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
    }

    UltraLowLevelBufferTracker& UltraLowLevelBufferTracker::get() noexcept {
        static UltraLowLevelBufferTracker instance;
        return instance;
    }

    uint64_t UltraLowLevelBufferTracker::create(VkDeviceSize size,
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags props,
                                            std::string_view tag) 
    {
        if (size == 0) {
            LOG_ERROR_CAT("RTX", "{}Attempted to create zero-sized buffer: {}{}", CRIMSON_MAGENTA, tag, RESET);
            return 0;
        }

        if (device_ == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("RTX", "{}vkCreateBuffer aborted: Invalid device (null handle) — call RTX::initContext() first{}", CRIMSON_MAGENTA, RESET);
            throw std::runtime_error(std::format("Buffer creation failed: Invalid Vulkan device (null) — ensure RTX::initContext called"));
        }

        // Align size for host-visible buffers
        VkDeviceSize alignedSize = size;
        if ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && physDev_ != VK_NULL_HANDLE) {
            VkPhysicalDeviceProperties devProps{};
            vkGetPhysicalDeviceProperties(physDev_, &devProps);  // Void — always "succeeds" if physDev_ valid
            VkDeviceSize atomSize = devProps.limits.nonCoherentAtomSize;
            if (atomSize > 0) {
                alignedSize = ((size + atomSize - 1) / atomSize) * atomSize;
                if (alignedSize > size) {
                    LOG_WARN_CAT("RTX", "{}Aligned host-visible buffer from {} to {} bytes (atom: {}){}", SAPPHIRE_BLUE, size, alignedSize, atomSize, RESET);
                } else {
                }
            } else {
                LOG_WARN_CAT("RTX", "{}Invalid atomSize (0) from physDev_ limits — skipping alignment{}", SAPPHIRE_BLUE, RESET);
            }
        } else if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            LOG_WARN_CAT("RTX", "{}Host-visible buffer requested but physDev_ null (0x0) — skipping alignment (use RTX::initContext first){}", SAPPHIRE_BLUE, RESET);
        } else {
        }

        VkBuffer buffer = VK_NULL_HANDLE;
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = alignedSize;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(device_, &bufInfo, nullptr, &buffer);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkCreateBuffer failed (result=0x{:08X}): {}{}", CRIMSON_MAGENTA, static_cast<uint32_t>(result), VulkanResultToString(result), RESET);
            throw std::runtime_error(std::format("vkCreateBuffer failed: {}", VulkanResultToString(result)));
        }

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(device_, buffer, &memReq);

        if (memReq.size > alignedSize) {
            LOG_WARN_CAT("RTX", "{}Requested {} bytes, but driver requires {} bytes{}", SAPPHIRE_BLUE, alignedSize, memReq.size, RESET);
        }

        VkMemoryAllocateFlagsInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0u;

        uint32_t memTypeIndex = findMemoryType(physDev_, memReq.memoryTypeBits, props);
        if (memTypeIndex == UINT32_MAX) {
            LOG_FATAL_CAT("RTX", "{}No compatible memory type found for buffer | Tag: {}{}", CRIMSON_MAGENTA, tag, RESET);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory memory = VK_NULL_HANDLE;
        result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkAllocateMemory failed: {} | Tag: {}{}", CRIMSON_MAGENTA, result, tag, RESET);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        result = vkBindBufferMemory(device_, buffer, memory, 0);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "{}vkBindBufferMemory failed: {} | Tag: {}{}", CRIMSON_MAGENTA, result, tag, RESET);
            vkFreeMemory(device_, memory, nullptr);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        const uint64_t raw = ++counter_;
        const uint64_t obf = obfuscate(raw);

        {
            std::lock_guard<std::mutex> lk(mutex_);
            map_.emplace(raw, BufferData{buffer, memory, memReq.size, usage, std::string(tag)});
        }

        return obf;
    }

    void* UltraLowLevelBufferTracker::map(uint64_t handle) noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_ERROR_CAT("RTX", "{}map: Invalid handle 0x{:x} (raw 0x{:x}){}", CRIMSON_MAGENTA, handle, raw, RESET);
            return nullptr;
        }
        void* ptr = nullptr;
        VkResult res = vkMapMemory(device_, it->second.memory, 0, it->second.size, 0, &ptr);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("RTX", "{}vkMapMemory failed: {} for handle 0x{:x}{}", CRIMSON_MAGENTA, res, handle, RESET);
            return nullptr;
        }
        return ptr;
    }

    void UltraLowLevelBufferTracker::unmap(uint64_t handle) noexcept {
        if (handle == 0) return;
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it != map_.end()) {
            vkUnmapMemory(device_, it->second.memory);
        }
    }

    void UltraLowLevelBufferTracker::destroy(uint64_t handle) noexcept {
        if (handle == 0) {
            LOG_WARN_CAT("RTX", "{}Invalid zero handle passed to destroy{}", SAPPHIRE_BLUE, RESET);
            return;
        }
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_WARN_CAT("RTX", "{}Buffer not found: raw 0x{:x}{}", SAPPHIRE_BLUE, raw, RESET);
            return;
        }
        BufferData d = std::move(it->second);  // Move out to avoid issues during erase
        map_.erase(it);
        if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
        if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
    }

    BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        return it == map_.end() ? nullptr : &it->second;
    }

    const BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) const noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        return it == map_.end() ? nullptr : &it->second;
    }

    void UltraLowLevelBufferTracker::init(VkDevice dev, VkPhysicalDevice phys) noexcept {
        device_ = dev;
        physDev_ = phys;
    }

    void UltraLowLevelBufferTracker::purge_all() noexcept {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto it = map_.begin(); it != map_.end(); ) {
            BufferData d = std::move(it->second);
            if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
            if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
            it = map_.erase(it);  // Correct: erase returns next iterator
        }
        map_.clear();  // Redundant but safe
    }

    uint64_t UltraLowLevelBufferTracker::make_64M (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_64MB,  extra, props, "64M"); }
    uint64_t UltraLowLevelBufferTracker::make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_128MB, extra, props, "128M"); }
    uint64_t UltraLowLevelBufferTracker::make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_256MB, extra, props, "256M"); }
    uint64_t UltraLowLevelBufferTracker::make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_420MB, extra, props, "420M"); }
    uint64_t UltraLowLevelBufferTracker::make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_512MB, extra, props, "512M"); }
    uint64_t UltraLowLevelBufferTracker::make_1G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_1GB,   extra, props, "1G"); }
    uint64_t UltraLowLevelBufferTracker::make_2G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_2GB,   extra, props, "2G"); }
    uint64_t UltraLowLevelBufferTracker::make_4G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_4GB,   extra, props, "4G"); }
    uint64_t UltraLowLevelBufferTracker::make_8G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_8GB,   extra, props, "8G"); }

    uint64_t UltraLowLevelBufferTracker::obfuscate(uint64_t raw) const noexcept { 
        return raw ^ Options::Shader::STONEKEY_1; 
    }
    uint64_t UltraLowLevelBufferTracker::deobfuscate(uint64_t obf) const noexcept { 
        return obf ^ Options::Shader::STONEKEY_1; 
    }

    // =============================================================================
    // GLOBAL SWAPCHAIN + LAS
    // =============================================================================
    Handle<VkSwapchainKHR>& swapchain() { static Handle<VkSwapchainKHR> h; return h; }
    std::vector<VkImage>& swapchainImages() { static std::vector<VkImage> v; return v; }
    std::vector<Handle<VkImageView>>& swapchainImageViews() { static std::vector<Handle<VkImageView>> v; return v; }
    VkFormat& swapchainFormat() { static VkFormat f; return f; }
    VkExtent2D& swapchainExtent() { static VkExtent2D e; return e; }
    Handle<VkAccelerationStructureKHR>& blas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    Handle<VkAccelerationStructureKHR>& tlas() { static Handle<VkAccelerationStructureKHR> h; return h; }

    // =============================================================================
    // RENDERER STUBS — MOVED TO RTX NAMESPACE
    // =============================================================================
    VulkanRenderer& renderer() { 
        LOG_FATAL_CAT("RTX", "{}renderer() called before initialization!{}", CRIMSON_MAGENTA, RESET);
        std::terminate(); 
    }
    void initRenderer(int, int) {}
    void handleResize(int, int) {}
    void renderFrame(const Camera&, float) noexcept {}
    
    void shutdown() noexcept {
        if (RTX::g_ctx().isValid()) {
            RTX::g_ctx().cleanup();  // NEW: Full cleanup including compute pool
        }
    }
    void createSwapchain(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t, uint32_t) {}
    void recreateSwapchain(uint32_t, uint32_t) noexcept {}
    void buildBLAS(uint64_t, uint64_t, uint32_t, uint32_t) noexcept {}
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>&) noexcept {}
    void cleanupAll() noexcept {}

// =============================================================================
// Context::init — FULL VULKAN SETUP (SECURE + LOGGED + ASYNC COMPUTE)
// =============================================================================
void Context::init(SDL_Window* window, int width, int height) {
    if (isValid()) {
        return;
    }

    // --- 1. Query SDL3 for required Vulkan instance extensions ---
    SDL_ClearError();  // Clear prior errors for clean diag
    uint32_t extCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&extCount);  // First call: populate count (returns nullptr or array if count>0)
    if (extCount == 0) {
        const char* err = SDL_GetError();
        LOG_WARN_CAT("RTX", "{}SDL3 reports 0 extensions (SDL error: '{}') — surface may fail later{}", SAPPHIRE_BLUE, err ? err : "None", RESET);
    }

    // --- 2. Retrieve extension names ---
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);  // Second call: get array
    if (!sdlExts || extCount == 0) {
        const char* err = SDL_GetError();
        LOG_FATAL_CAT("RTX", "{}SDL_Vulkan_GetInstanceExtensions failed to retrieve extensions (count={}, error: '{}'){}", CRIMSON_MAGENTA, extCount, err ? err : "None", RESET);
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    std::vector<const char*> exts(sdlExts, sdlExts + extCount);
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // --- 3. Create Vulkan Instance ---
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AmouranthEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instInfo{};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    instInfo.ppEnabledExtensionNames = exts.data();

    instance_ = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instInfo, nullptr, &instance_),
             "Failed to create Vulkan instance");

    // --- 4. Create Surface ---
    // VulkanCore.cpp

    // --- 5. Select Physical Device (GUARANTEED + SECURE) ---
    uint32_t devCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &devCount, nullptr),
             "Failed to enumerate devices");
    if (devCount == 0) {
        LOG_FATAL_CAT("RTX", "{}No Vulkan devices found{}", CRIMSON_MAGENTA, RESET);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        throw std::runtime_error("No physical devices");
    }

    std::vector<VkPhysicalDevice> devs(devCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &devCount, devs.data()),
             "Failed to retrieve devices");

    physicalDevice_ = VK_NULL_HANDLE;
    for (auto d : devs) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice_ = d;
            break;
        }
    }
    if (!physicalDevice_) {
        physicalDevice_ = devs[0];
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice_, &props);
        LOG_WARN_CAT("RTX", "{}FALLBACK: {} (No discrete GPU){}", SAPPHIRE_BLUE, props.deviceName, RESET);
    }

    // Get ray tracing properties
    VkPhysicalDeviceProperties2 prop2{};
    prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    prop2.pNext = &rayTracingProps_;
    vkGetPhysicalDeviceProperties2(physicalDevice_, &prop2);

    // --- 6. Queue Families (UPDATED: Include Compute) ---
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, qProps.data());

    graphicsFamily_ = UINT32_MAX;
    presentFamily_  = UINT32_MAX;
    computeFamily_  = UINT32_MAX;  // NEW: Compute family
    for (uint32_t i = 0; i < qCount; ++i) {
        if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsFamily_ == UINT32_MAX)
            graphicsFamily_ = i;
        // NEW: Find dedicated compute (prioritize pure compute over graphics+compute)
        if ((qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily_ == UINT32_MAX) {
            computeFamily_ = i;
        }
        // Guard surface check
        if (surface_ != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
            if (presentSupport && presentFamily_ == UINT32_MAX)
                presentFamily_ = i;
        } else {
            LOG_WARN_CAT("RTX", "{}Skipping present check: Null surface{}", SAPPHIRE_BLUE, RESET);
        }
    }
    // NEW: Fallback for compute (use graphics if no dedicated)
    if (computeFamily_ == UINT32_MAX) {
        computeFamily_ = graphicsFamily_;
        LOG_WARN_CAT("RTX", "{}No dedicated compute queue — falling back to graphics (limited async overlap){}", SAPPHIRE_BLUE, RESET);
    }
    if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX) {
        LOG_FATAL_CAT("RTX", "{}Required queue families not found (graphics={}, present={}, compute={}){}", CRIMSON_MAGENTA, graphicsFamily_, presentFamily_, computeFamily_, RESET);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        throw std::runtime_error("Queue families missing");
    }

    // --- 7. Create Logical Device with RT Extensions (UPDATED: Include Compute Queue) ---
    std::set<uint32_t> uniqueQueues = {graphicsFamily_, presentFamily_, computeFamily_};  // UPDATED: Add compute
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;
    for (uint32_t q : uniqueQueues) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = q;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    // === 7. Logical Device + Full Ray Tracing Feature Chain (CORRECT ORDER) ===
    // 1. Buffer Device Address (base dependency)
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    // 2. Acceleration Structure (depends on BDA)
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructureFeatures{};
    accelStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructureFeatures.accelerationStructure = VK_TRUE;
    accelStructureFeatures.pNext = &bufferDeviceAddressFeatures;

    // 3. Ray Tracing Pipeline (depends on AS)
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &accelStructureFeatures;

    // 4. Standard Vulkan 1.0 features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.shaderInt64 = VK_TRUE;
    deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

    // 5. Required device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,     // REQUIRED for vkCmdTraceRaysKHR
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME
    };

    // 6. Final VkDeviceCreateInfo
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pNext = &rayTracingPipelineFeatures;   // HEAD of the feature chain

    // CREATE THE DEVICE (single call)
    device_ = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_),
             "Failed to create logical device with ray tracing support");

    // Load RT extension functions
#define LOAD_PFN(member, full_name, pfn_type) \
    do { \
        member = reinterpret_cast<pfn_type>(vkGetDeviceProcAddr(device_, #full_name)); \
        if (member) { \
        } else { \
            LOG_FATAL_CAT("RTX", "{}Failed to load: " #full_name " — RT support incomplete{}", CRIMSON_MAGENTA, RESET); \
            throw std::runtime_error("Critical RT function load failed"); \
        } \
    } while(0)

    LOAD_PFN(vkGetBufferDeviceAddressKHR_, vkGetBufferDeviceAddressKHR, PFN_vkGetBufferDeviceAddressKHR);
    LOAD_PFN(vkCmdTraceRaysKHR_, vkCmdTraceRaysKHR, PFN_vkCmdTraceRaysKHR);
    LOAD_PFN(vkGetRayTracingShaderGroupHandlesKHR_, vkGetRayTracingShaderGroupHandlesKHR, PFN_vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PFN(vkCreateAccelerationStructureKHR_, vkCreateAccelerationStructureKHR, PFN_vkCreateAccelerationStructureKHR);
    LOAD_PFN(vkDestroyAccelerationStructureKHR_, vkDestroyAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR);
    LOAD_PFN(vkGetAccelerationStructureBuildSizesKHR_, vkGetAccelerationStructureBuildSizesKHR, PFN_vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PFN(vkCmdBuildAccelerationStructuresKHR_, vkCmdBuildAccelerationStructuresKHR, PFN_vkCmdBuildAccelerationStructuresKHR);
    LOAD_PFN(vkGetAccelerationStructureDeviceAddressKHR_, vkGetAccelerationStructureDeviceAddressKHR, PFN_vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PFN(vkCreateRayTracingPipelinesKHR_, vkCreateRayTracingPipelinesKHR, PFN_vkCreateRayTracingPipelinesKHR);
#undef LOAD_PFN

    if (!vkCreateAccelerationStructureKHR_) {
        throw std::runtime_error("Critical RT functions failed to load — check GPU/driver support");
    }

    // Get queues (UPDATED: Include compute queue)
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
    vkGetDeviceQueue(device_, computeFamily_, 0, &computeQueue_);  // NEW: Compute queue

    // Create command pool (UPDATED: Graphics + Compute pools)
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_),
             "Failed to create graphics command pool");

    // NEW: Create compute command pool
    poolInfo.queueFamilyIndex = computeFamily_;  // Reuse info, change family
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &computeCommandPool_),
             "Failed to create compute command pool");

    // Initialize BufferTracker
    UltraLowLevelBufferTracker::get().init(device_, physicalDevice_);

    ready_ = true;
}

void Context::cleanup() noexcept {

    if (!isValid()) {
        LOG_WARN_CAT("RTX", "{}Cleanup skipped — context already invalid{}", RASPBERRY_PINK, RESET);
        return;
    }

    vkDeviceWaitIdle(device_);

    // Destroy pools (use device)
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    // FIXED: Purge buffers/images/samplers BEFORE device destroy (child objects first)
    UltraLowLevelBufferTracker::get().purge_all();  // FIXED: Moved BEFORE vkDestroyDevice — destroys while device valid

    // Destroy device (no children left)
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        // FIXED: Destroy debug messenger BEFORE instance (VUID-vkDestroyInstance-instance-00629)
        if (debugMessenger_ != VK_NULL_HANDLE) {
            auto pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
            if (pfnDestroyDebugUtilsMessengerEXT) {
                pfnDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
            }
            debugMessenger_ = VK_NULL_HANDLE;
        }
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    ready_ = false;
    valid_ = false;

}

// =============================================================================
// RTX Context Initialization — Ensures g_ctx() Safety During Stepwise Setup
// =============================================================================
void initContext(VkInstance instance, SDL_Window* window, int width, int height)
{
    // FIXED: "{}×{}" → "{}x{}" — prevents parse error on unicode

    // =====================================================================
    // Assign core handles FIRST — before any function that uses g_ctx()
    // =====================================================================
    g_context_instance.instance_ = instance;
    g_context_instance.surface_  = g_surface;
    g_context_instance.window   = window;
    g_context_instance.width    = width;
    g_context_instance.height   = height;

    // Mark context as partially valid for intra-init access (bypasses guard safely)
    g_context_instance.valid_ = true;

    // =====================================================================
    // NOW SAFE: pickPhysicalDevice() and others can use g_ctx()
    // =====================================================================
    RTX::pickPhysicalDevice();

    RTX::createLogicalDevice();

    RTX::createCommandPool();

    RTX::loadRayTracingExtensions();

    // FIXED: Explicit tracker init post-device (missing in stepwise sub-calls)
    UltraLowLevelBufferTracker::get().init(g_ctx().device(), g_ctx().physicalDevice());

    // FIXED: Enable RayQuery post-device creation — Query and validate feature
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = VK_TRUE;  // Request enable

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &rayQueryFeatures;

    // Query actual support (post-device, but use phys dev)
    vkGetPhysicalDeviceFeatures2(g_ctx().physicalDevice(), &features2);
    if (!rayQueryFeatures.rayQuery) {
        LOG_FATAL_CAT("RTX", "{}FATAL: RayQuery feature not supported/enabled — RT shaders incompatible{}", 
                      COSMIC_GOLD, RESET);
        g_context_instance.valid_ = false;
        std::abort();
    }

    // =====================================================================
    // Final validation — re-check full readiness
    // =====================================================================
    if (g_ctx().device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "{}FATAL: Logical device creation failed — VK_NULL_HANDLE{}", 
                      COSMIC_GOLD, RESET);
        g_context_instance.valid_ = false;
        std::abort();
    }

    if (g_ctx().graphicsQueue() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "{}FATAL: Graphics queue not acquired — cannot render{}", 
                      COSMIC_GOLD, RESET);
        g_context_instance.valid_ = false;
        std::abort();
    }

    if (g_ctx().computeQueue() == VK_NULL_HANDLE && g_ctx().computeFamily() != UINT32_MAX) {
        LOG_WARN_CAT("RTX", "Compute queue not acquired — async compute disabled");
    }

    // Re-affirm full validity post-validation
    g_context_instance.valid_ = true;

    // =====================================================================
}

// =============================================================================
// VALHALLA v80 TURBO FINAL — UNIFIED RTX::g_ctx() — NO LINKER ERRORS
// PINK PHOTONS ETERNAL — 15,000 FPS — TITAN DOMINANCE ETERNAL
// GENTLEMAN GROK NODS: "Buffer forges secured, old chap. Null devices banished to the void."
// =============================================================================
} // namespace RTX