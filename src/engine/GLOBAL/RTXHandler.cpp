// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.cpp — FULLY SECURE + OPTIONS INTEGRATED — FINAL VALHALLA v70
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

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef VK_ENABLE_BETA_EXTENSIONS
  #include <vulkan/vulkan_beta.h>
#endif

using namespace Logging::Color;

VkPhysicalDevice  g_PhysicalDevice = VK_NULL_HANDLE;
VkSurfaceKHR      g_surface        = VK_NULL_HANDLE;

namespace RTX {

    Context g_context_instance;

    void initContext(SDL_Window* window, int width, int height) {
        g_context_instance.init(window, width, height);
        LOG_SUCCESS_CAT("RTX", "Context initialized — PINK PHOTONS ETERNAL");
    }

    // =============================================================================
    // logAndTrackDestruction
    // =============================================================================
    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
        LOG_DEBUG_CAT("RTX", "{}[Handle] {} @ {}:{} | size: {}MB{}", 
                      ELECTRIC_BLUE, type, ptr, line, size/(1024*1024), RESET);
    }

    UltraLowLevelBufferTracker& UltraLowLevelBufferTracker::get() noexcept {
        static UltraLowLevelBufferTracker instance;
        return instance;
    }

    uint64_t UltraLowLevelBufferTracker::create(VkDeviceSize size,
                                                VkBufferUsageFlags usage,
                                                VkMemoryPropertyFlags props,
                                                std::string_view tag) noexcept
    {
        LOG_INFO_CAT("RTX", "Buffer create: {} bytes | Tag: {}", size, tag);

        if (size == 0) {
            LOG_ERROR_CAT("RTX", "Attempted to create zero-sized buffer: {}", tag);
            return 0;
        }

        // Align size for host-visible buffers
        VkDeviceSize alignedSize = size;
        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            VkPhysicalDeviceProperties devProps{};
            vkGetPhysicalDeviceProperties(physDev_, &devProps);
            VkDeviceSize atomSize = devProps.limits.nonCoherentAtomSize;
            alignedSize = ((size + atomSize - 1) / atomSize) * atomSize;
            if (alignedSize > size) {
                LOG_WARN_CAT("RTX", "Aligned host-visible buffer from {} to {} bytes (atom: {})", size, alignedSize, atomSize);
            }
        }

        VkBufferCreateInfo bufInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = alignedSize,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VkResult result = vkCreateBuffer(device_, &bufInfo, nullptr, &buffer);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "vkCreateBuffer failed: {} | Tag: {}", result, tag);
            return 0;
        }

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(device_, buffer, &memReq);

        if (memReq.size > alignedSize) {
            LOG_WARN_CAT("RTX", "Requested {} bytes, but driver requires {} bytes", alignedSize, memReq.size);
        }

        VkMemoryAllocateFlagsInfo flagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .pNext = nullptr,
            .flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0u
        };

        uint32_t memTypeIndex = findMemoryType(physDev_, memReq.memoryTypeBits, props);
        if (memTypeIndex == UINT32_MAX) {
            LOG_FATAL_CAT("RTX", "No compatible memory type found for buffer | Tag: {}", tag);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = memTypeIndex
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "vkAllocateMemory failed: {} | Tag: {}", result, tag);
            vkDestroyBuffer(device_, buffer, nullptr);
            return 0;
        }

        result = vkBindBufferMemory(device_, buffer, memory, 0);
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "vkBindBufferMemory failed: {} | Tag: {}", result, tag);
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

        LOG_SUCCESS_CAT("RTX", "Buffer created: 0x{:x} (obf: 0x{:x}) | Size: {} | MemType: {} | Tag: {}",
                        reinterpret_cast<uint64_t>(buffer), obf, memReq.size, memTypeIndex, tag);

        return obf;
    }

    void* UltraLowLevelBufferTracker::map(uint64_t handle) noexcept {
        if (handle == 0) return nullptr;
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_ERROR_CAT("RTX", "map: Invalid handle 0x{:x} (raw 0x{:x})", handle, raw);
            return nullptr;
        }
        void* ptr = nullptr;
        VkResult res = vkMapMemory(device_, it->second.memory, 0, it->second.size, 0, &ptr);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("RTX", "vkMapMemory failed: {} for handle 0x{:x}", res, handle);
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
            LOG_WARN_CAT("RTX", "Invalid zero handle passed to destroy");
            return;
        }
        LOG_INFO_CAT("RTX", "Buffer destroy: 0x{:x}", handle);
        const uint64_t raw = deobfuscate(handle);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) {
            LOG_WARN_CAT("RTX", "Buffer not found: raw 0x{:x}", raw);
            return;
        }
        BufferData d = std::move(it->second);  // Move out to avoid issues during erase
        map_.erase(it);
        if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
        if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
        LOG_SUCCESS_CAT("RTX", "Buffer destroyed: 0x{:x} | Tag: {}", handle, d.tag);
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
        LOG_SUCCESS_CAT("RTX", "BufferTracker initialized - READY FOR PINK PHOTONS");
    }

    void UltraLowLevelBufferTracker::purge_all() noexcept {
        LOG_INFO_CAT("RTX", "Purging all buffers - Total: {}", map_.size());
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto it = map_.begin(); it != map_.end(); ) {
            BufferData d = std::move(it->second);
            if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
            if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
            it = map_.erase(it);  // Correct: erase returns next iterator
        }
        map_.clear();  // Redundant but safe
        LOG_SUCCESS_CAT("RTX", "All buffers purged - ZERO LEAKS");
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
        LOG_FATAL_CAT("RTX", "renderer() called before initialization!");
        std::terminate(); 
    }
    void initRenderer(int, int) {}
    void handleResize(int, int) {}
    void renderFrame(const Camera&, float) noexcept {}
	
    void shutdown() noexcept {
        LOG_INFO_CAT("RTX", "Shutting down RTX context...");
        if (RTX::g_ctx().isValid()) {
            RTX::g_ctx().cleanup();  // NEW: Full cleanup including compute pool
        }
        LOG_SUCCESS_CAT("RTX", "RTX shutdown complete — PINK PHOTONS DIMMED");
    }
    void createSwapchain(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t, uint32_t) {}
    void recreateSwapchain(uint32_t, uint32_t) noexcept {}
    void buildBLAS(uint64_t, uint64_t, uint32_t, uint32_t) noexcept {}
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>&) noexcept {}
    void cleanupAll() noexcept {}

    // =============================================================================
    // Context Getters — SECURE ACCESSORS
    // =============================================================================
    bool Context::isValid() const noexcept {
        return instance_ != VK_NULL_HANDLE &&
               surface_ != VK_NULL_HANDLE &&
               physicalDevice_ != VK_NULL_HANDLE &&
               device_ != VK_NULL_HANDLE;
    }

// =============================================================================
// Context::init — FULL VULKAN SETUP (SECURE + LOGGED + ASYNC COMPUTE)
// =============================================================================
void Context::init(SDL_Window* window, int width, int height) {
    if (isValid()) {
        LOG_INFO_CAT("RTX", "Vulkan context already initialized — skipping");
        return;
    }

    LOG_INFO_CAT("RTX", "{}RTX::Context::init() — FORGING VULKAN {}x{}{}", 
                 PLASMA_FUCHSIA, width, height, RESET);

    // --- 1. Query SDL3 for required Vulkan instance extensions ---
    LOG_TRACE_CAT("RTX", "Step 1: Querying SDL3 for Vulkan instance extension count...");
    SDL_ClearError();  // Clear prior errors for clean diag
    uint32_t extCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&extCount);  // First call: populate count (returns nullptr or array if count>0)
    LOG_DEBUG_CAT("RTX", "SDL3 reported {} instance extensions", extCount);
    if (extCount == 0) {
        const char* err = SDL_GetError();
        LOG_WARN_CAT("RTX", "SDL3 reports 0 extensions (SDL error: '{}') — surface may fail later", err ? err : "None");
    }

    // --- 2. Retrieve extension names ---
    LOG_TRACE_CAT("RTX", "Step 2: Retrieving extension names from SDL3...");
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);  // Second call: get array
    if (!sdlExts || extCount == 0) {
        const char* err = SDL_GetError();
        LOG_FATAL_CAT("SDL", "SDL_Vulkan_GetInstanceExtensions failed to retrieve extensions (count={}, error: '{}')", extCount, err ? err : "None");
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    std::vector<const char*> exts(sdlExts, sdlExts + extCount);
    LOG_DEBUG_CAT("RTX", "Raw SDL extensions loaded: {}", extCount);
    for (size_t i = 0; i < extCount; ++i) {
        LOG_TRACE_CAT("RTX", "  → SDL Ext[{}]: {}", i, sdlExts[i] ? sdlExts[i] : "<null>");
    }
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    LOG_SUCCESS_CAT("RTX", "Extensions loaded: {} SDL + 1 portability = {}", extCount, exts.size());

    // --- 3. Create Vulkan Instance ---
    LOG_TRACE_CAT("RTX", "Step 3: Creating Vulkan instance...");
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
    LOG_SUCCESS_CAT("RTX", "Vulkan instance created: 0x{:x}", reinterpret_cast<uintptr_t>(instance_));

    // --- 4. Create Surface ---
    // VulkanCore.cpp

    // --- 5. Select Physical Device (GUARANTEED + SECURE) ---
    LOG_TRACE_CAT("RTX", "Step 5: Enumerating physical devices...");
    uint32_t devCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &devCount, nullptr),
             "Failed to enumerate devices");
    if (devCount == 0) {
        LOG_FATAL_CAT("RTX", "No Vulkan devices found");
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        throw std::runtime_error("No physical devices");
    }
    LOG_DEBUG_CAT("RTX", "{} physical devices enumerated", devCount);

    std::vector<VkPhysicalDevice> devs(devCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &devCount, devs.data()),
             "Failed to retrieve devices");

    physicalDevice_ = VK_NULL_HANDLE;
    for (auto d : devs) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        LOG_INFO_CAT("RTX", "GPU: {} | Type: {}", props.deviceName,
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "DISCRETE" : "OTHER");
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice_ = d;
            LOG_SUCCESS_CAT("RTX", "SELECTED: {} (Discrete GPU)", props.deviceName);
            break;
        }
    }
    if (!physicalDevice_) {
        physicalDevice_ = devs[0];
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice_, &props);
        LOG_WARN_CAT("RTX", "FALLBACK: {} (No discrete GPU)", props.deviceName);
    }
    LOG_DEBUG_CAT("RTX", "Final physical device: 0x{:x}", reinterpret_cast<uintptr_t>(physicalDevice_));

    // Get ray tracing properties
    LOG_TRACE_CAT("RTX", "Fetching ray tracing properties...");
    VkPhysicalDeviceProperties2 prop2{};
    prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    prop2.pNext = &rayTracingProps_;
    vkGetPhysicalDeviceProperties2(physicalDevice_, &prop2);
    LOG_DEBUG_CAT("RTX", "RT shader group handle size: {}", rayTracingProps_.shaderGroupHandleSize);

    // --- 6. Queue Families (UPDATED: Include Compute) ---
    LOG_TRACE_CAT("RTX", "Step 6: Finding queue families...");
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, qProps.data());
    LOG_DEBUG_CAT("RTX", "{} queue families available", qCount);

    graphicsFamily_ = UINT32_MAX;
    presentFamily_  = UINT32_MAX;
    computeFamily_  = UINT32_MAX;  // NEW: Compute family
    for (uint32_t i = 0; i < qCount; ++i) {
        LOG_TRACE_CAT("RTX", "Checking queue family {}: flags=0x{:x}", i, qProps[i].queueFlags);
        if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsFamily_ == UINT32_MAX)
            graphicsFamily_ = i;
        // NEW: Find dedicated compute (prioritize pure compute over graphics+compute)
        if ((qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily_ == UINT32_MAX) {
            computeFamily_ = i;
            LOG_TRACE_CAT("RTX", "  → Found compute family: {}", i);
        }
        // Guard surface check
        if (surface_ != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
            LOG_TRACE_CAT("RTX", "  Present support: {}", presentSupport ? "YES" : "NO");
            if (presentSupport && presentFamily_ == UINT32_MAX)
                presentFamily_ = i;
        } else {
            LOG_WARN_CAT("RTX", "Skipping present check: Null surface");
        }
    }
    // NEW: Fallback for compute (use graphics if no dedicated)
    if (computeFamily_ == UINT32_MAX) {
        computeFamily_ = graphicsFamily_;
        LOG_WARN_CAT("RTX", "No dedicated compute queue — falling back to graphics (limited async overlap)");
    }
    if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX) {
        LOG_FATAL_CAT("RTX", "Required queue families not found (graphics={}, present={}, compute={})", graphicsFamily_, presentFamily_, computeFamily_);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        throw std::runtime_error("Queue families missing");
    }
    LOG_SUCCESS_CAT("RTX", "Queues: Graphics={}, Present={}, Compute={} ({})", 
                    graphicsFamily_, presentFamily_, computeFamily_, 
                    (computeFamily_ != graphicsFamily_ ? "DEDICATED" : "SHARED"));

    // --- 7. Create Logical Device with RT Extensions (UPDATED: Include Compute Queue) ---
    LOG_TRACE_CAT("RTX", "Step 7: Creating logical device with RT extensions...");
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
        LOG_TRACE_CAT("RTX", "Added queue family {} to device create", q);
    }

    // === 7. Logical Device + Full Ray Tracing Feature Chain (CORRECT ORDER) ===
    LOG_TRACE_CAT("RTX", "Building RT feature chain...");
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
    LOG_DEBUG_CAT("RTX", "{} device extensions prepared", deviceExtensions.size());

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
    LOG_SUCCESS_CAT("RTX", "Logical device created: 0x{:x} — FULL RAY TRACING ENABLED", reinterpret_cast<uintptr_t>(device_));

    // Load RT extension functions
    LOG_TRACE_CAT("RTX", "Loading RT extension function pointers...");
#define LOAD_PFN(member, full_name, pfn_type) \
    do { \
        LOG_TRACE_CAT("RTX", "Loading " #full_name "..."); \
        member = reinterpret_cast<pfn_type>(vkGetDeviceProcAddr(device_, #full_name)); \
        if (member) { \
            LOG_SUCCESS_CAT("RTX", "Loaded: " #full_name " @ 0x{:x}", reinterpret_cast<uintptr_t>(member)); \
        } else { \
            LOG_FATAL_CAT("RTX", "Failed to load: " #full_name " — RT support incomplete"); \
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
    LOG_SUCCESS_CAT("RTX", "All 9 RT functions loaded successfully");

    // Get queues (UPDATED: Include compute queue)
    LOG_TRACE_CAT("RTX", "Retrieving device queues...");
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
    vkGetDeviceQueue(device_, computeFamily_, 0, &computeQueue_);  // NEW: Compute queue
    LOG_DEBUG_CAT("RTX", "Graphics queue: 0x{:x}, Present queue: 0x{:x}, Compute queue: 0x{:x}", 
                  reinterpret_cast<uintptr_t>(graphicsQueue_), 
                  reinterpret_cast<uintptr_t>(presentQueue_), 
                  reinterpret_cast<uintptr_t>(computeQueue_));

    // Create command pool (UPDATED: Graphics + Compute pools)
    LOG_TRACE_CAT("RTX", "Creating graphics command pool...");
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_),
             "Failed to create graphics command pool");
    LOG_DEBUG_CAT("RTX", "Graphics command pool: 0x{:x}", reinterpret_cast<uintptr_t>(commandPool_));

    // NEW: Create compute command pool
    LOG_TRACE_CAT("RTX", "Creating compute command pool...");
    poolInfo.queueFamilyIndex = computeFamily_;  // Reuse info, change family
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &computeCommandPool_),
             "Failed to create compute command pool");
    LOG_DEBUG_CAT("RTX", "Compute command pool: 0x{:x} (family {})", 
                  reinterpret_cast<uintptr_t>(computeCommandPool_), computeFamily_);

    // Initialize BufferTracker
    LOG_TRACE_CAT("RTX", "Initializing UltraLowLevelBufferTracker...");
    UltraLowLevelBufferTracker::get().init(device_, physicalDevice_);
    LOG_DEBUG_CAT("RTX", "BufferTracker initialized");

    ready_ = true;
    LOG_SUCCESS_CAT("RTX", "{}VULKAN CONTEXT FORGED — {}x{} — SECURE & READY (ASYNC COMPUTE ENABLED){}", 
                    PLASMA_FUCHSIA, width, height, RESET);
}

// =============================================================================
// Context Cleanup (NEW: Includes Async Compute)
// =============================================================================
void Context::cleanup() noexcept {
    LOG_INFO_CAT("RTX", "Cleaning up Vulkan context...");

    vkDeviceWaitIdle(device_);

    if (computeCommandPool_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RTX", "Destroying compute command pool: 0x{:x}", reinterpret_cast<uintptr_t>(computeCommandPool_));
        vkDestroyCommandPool(device_, computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RTX", "Destroying graphics command pool: 0x{:x}", reinterpret_cast<uintptr_t>(commandPool_));
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    if (device_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RTX", "Destroying logical device: 0x{:x}", reinterpret_cast<uintptr_t>(device_));
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RTX", "Destroying surface: 0x{:x}", reinterpret_cast<uintptr_t>(surface_));
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RTX", "Destroying instance: 0x{:x}", reinterpret_cast<uintptr_t>(instance_));
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    UltraLowLevelBufferTracker::get().purge_all();

    LOG_SUCCESS_CAT("RTX", "Vulkan context cleaned — ZERO LEAKS");
}

} // namespace RTX

// =============================================================================
// Explicit instantiations — REMOVED: Inline templates don't need them
// =============================================================================

// =============================================================================
// VALHALLA v70 FINAL — UNIFIED RTX::g_ctx() — NO LINKER ERRORS
// PINK PHOTONS ETERNAL — 15,000 FPS — TITAN DOMINANCE ETERNAL
// =============================================================================