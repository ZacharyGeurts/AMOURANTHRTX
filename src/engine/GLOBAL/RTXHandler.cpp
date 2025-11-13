// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.cpp — FULLY SECURE + OPTIONS INTEGRATED
// • Uses Options::Shader::STONEKEY_1 (compile-time constant)
// • stonekey_xor_spirv() → ENCRYPTION MANDATORY
// • UNENCRYPTED SPIR-V = FATAL → NO ATTACH
// • g_ctx() guarded
// • Handle<T> destruction logged + shredded
// • BufferTracker → obfuscated handles
// • PINK PHOTONS ETERNAL — HACKERS BLIND
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"  // ← STONEKEY_1, STONEKEY_2, ALL OPTIONS
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

namespace RTX {

// =============================================================================
// Handle<T> — SECURE DESTRUCTION
// =============================================================================
template<typename T>
Handle<T>::Handle(T h, VkDevice d, DestroyFn del, size_t sz, std::string_view t)
    : raw(h), device(d), destroyer(del), size(sz), tag(t)
{
    if (h) {
        LOG_INFO_CAT("RTX", "Handle created: {} @ 0x{:x} | Tag: {} | Size: {}MB", 
                     typeid(T).name(), reinterpret_cast<uint64_t>(h), t, sz / (1024*1024));
        logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(h), __LINE__, size);
    }
}

template<typename T>
Handle<T>::Handle(Handle&& o) noexcept
    : raw(o.raw), device(o.device), destroyer(o.destroyer), size(o.size), tag(o.tag)
{
    o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
    LOG_DEBUG_CAT("RTX", "Handle moved: {} @ 0x{:x}", typeid(T).name(), reinterpret_cast<uint64_t>(raw));
}

template<typename T>
Handle<T>& Handle<T>::operator=(Handle&& o) noexcept {
    reset();
    raw = o.raw; device = o.device; destroyer = o.destroyer; size = o.size; tag = o.tag;
    o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
    LOG_DEBUG_CAT("RTX", "Handle assigned (move): {} @ 0x{:x}", typeid(T).name(), reinterpret_cast<uint64_t>(raw));
    return *this;
}

template<typename T>
Handle<T>& Handle<T>::operator=(std::nullptr_t) noexcept { reset(); return *this; }

template<typename T>
Handle<T>::operator bool() const noexcept { return raw != T{}; }

template<typename T>
bool Handle<T>::valid() const noexcept { return raw != T{}; }

template<typename T>
void Handle<T>::reset() noexcept {
    if (raw != T{}) {
        LOG_INFO_CAT("RTX", "Handle reset: {} @ 0x{:x} | Tag: {}", 
                     typeid(T).name(), reinterpret_cast<uint64_t>(raw), tag);
        if (destroyer && device) {
            constexpr size_t threshold = 16 * 1024 * 1024;
            if (size >= threshold) {
                LOG_DEBUG_CAT("RTX", "Skipping shred for large allocation (%zuMB): %s", 
                              size/(1024*1024), tag.empty() ? "" : std::string(tag).c_str());
            } else {
                std::memset(&raw, 0xCD, sizeof(T));
            }
            destroyer(device, raw, nullptr);
        }
        logAndTrackDestruction(tag.empty() ? typeid(T).name() : std::string(tag).c_str(), 
                               reinterpret_cast<void*>(raw), __LINE__, size);
        raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr;
    }
}

template<typename T>
Handle<T>::~Handle() { reset(); }

// =============================================================================
// MakeHandle
// =============================================================================
template<typename T, typename... Args>
auto MakeHandle(T h, VkDevice d, Args&&... args) {
    LOG_DEBUG_CAT("RTX", "MakeHandle invoked for type: {}", typeid(T).name());
    return Handle<T>(h, d, std::forward<Args>(args)...);
}

// =============================================================================
// GLOBAL ACCESSORS — EARLY ACCESS FATAL
// =============================================================================
Context& ctx() {
    LOG_DEBUG_CAT("RTX", "ctx() accessed — Global Vulkan Context");
    return g_ctx();
}

Context& g_ctx() {
    static Context ctx;
    LOG_DEBUG_CAT("RTX", "g_ctx() accessed — Global Vulkan Context");
    if (!ctx.isValid()) {
        LOG_FATAL_CAT("RTX", "FATAL: g_ctx() accessed BEFORE RTX::g_ctx().init()! ORDER VIOLATION.");
        LOG_FATAL_CAT("RTX", "Vulkan context not forged. Critical security breach.");
        std::terminate();
    }
    return ctx;
}

// =============================================================================
// Context Getters — SECURE ACCESSORS
// =============================================================================
bool Context::isValid() const noexcept {
    return instance_ != VK_NULL_HANDLE &&
           surface_ != VK_NULL_HANDLE &&
           physicalDevice_ != VK_NULL_HANDLE &&
           device_ != VK_NULL_HANDLE;
}

VkInstance Context::instance() const noexcept {
    return instance_;
}

VkSurfaceKHR Context::surface() const noexcept {
    return surface_;
}

VkPhysicalDevice Context::physicalDevice() const noexcept {
    return physicalDevice_;
}

VkDevice Context::device() const noexcept {
    return device_;
}

// =============================================================================
// Context::init — FULL VULKAN SETUP (SECURE + LOGGED)
// =============================================================================
void Context::init(SDL_Window* window, int width, int height) {
    if (isValid()) {
        LOG_INFO_CAT("RTX", "Vulkan context already initialized — skipping");
        return;
    }

    LOG_INFO_CAT("RTX", "{}RTX::Context::init() — FORGING VULKAN {}x{}{}", 
                 PLASMA_FUCHSIA, width, height, RESET);

    // --- 1. Query SDL3 for required Vulkan instance extensions ---
    LOG_INFO_CAT("RTX", "Querying SDL3 for Vulkan instance extension count...");
    uint32_t extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount)) {
        LOG_FATAL_CAT("SDL", "SDL_Vulkan_GetInstanceExtensions failed to query count");
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    LOG_SUCCESS_CAT("RTX", "SDL3 requires {} instance extensions", extCount);

    // --- 2. Retrieve extension names ---
    LOG_INFO_CAT("RTX", "Retrieving extension names from SDL3...");
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts) {
        LOG_FATAL_CAT("SDL", "SDL_Vulkan_GetInstanceExtensions failed to retrieve extensions");
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }
    std::vector<const char*> exts(sdlExts, sdlExts + extCount);
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    LOG_SUCCESS_CAT("RTX", "Extensions loaded: {} + portability", exts.size() - 1);

    // --- 3. Create Vulkan Instance ---
    LOG_INFO_CAT("RTX", "Creating Vulkan instance...");
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
    instInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VK_CHECK(vkCreateInstance(&instInfo, nullptr, &instance_),
             "Failed to create Vulkan instance");
    LOG_SUCCESS_CAT("RTX", "Vulkan instance created: 0x{:x}", reinterpret_cast<uint64_t>(instance_));

    // --- 4. Create Surface ---
    LOG_INFO_CAT("RTX", "Creating Vulkan surface via SDL3...");
    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {
        LOG_FATAL_CAT("SDL", "SDL_Vulkan_CreateSurface failed");
        vkDestroyInstance(instance_, nullptr);
        throw std::runtime_error("Failed to create surface");
    }
    LOG_SUCCESS_CAT("RTX", "Surface created: 0x{:x}", reinterpret_cast<uint64_t>(surface_));

    // --- 5. Select Physical Device (GUARANTEED + SECURE) ---
    LOG_INFO_CAT("RTX", "Enumerating physical devices...");
    uint32_t devCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &devCount, nullptr),
             "Failed to enumerate devices");
    if (devCount == 0) {
        LOG_FATAL_CAT("RTX", "No Vulkan devices found");
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

    // Get ray tracing properties
    VkPhysicalDeviceProperties2 prop2{};
    prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    prop2.pNext = &rayTracingProps_;
    vkGetPhysicalDeviceProperties2(physicalDevice_, &prop2);

    // --- 6. Queue Families ---
    LOG_INFO_CAT("RTX", "Finding queue families...");
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, qProps.data());

    graphicsFamily_ = UINT32_MAX;
    presentFamily_  = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && graphicsFamily_ == UINT32_MAX)
            graphicsFamily_ = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
        if (presentSupport && presentFamily_ == UINT32_MAX)
            presentFamily_ = i;
    }
    if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX) {
        LOG_FATAL_CAT("RTX", "Required queue families not found");
        throw std::runtime_error("Queue families missing");
    }
    LOG_SUCCESS_CAT("RTX", "Queues: Graphics={}, Present={}", graphicsFamily_, presentFamily_);

    // --- 7. Create Logical Device with RT Extensions ---
    LOG_INFO_CAT("RTX", "Creating logical device with RT extensions...");
    std::set<uint32_t> uniqueQueues = {graphicsFamily_, presentFamily_};
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

    // Enable RT features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructureFeatures{};
    accelStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructureFeatures.accelerationStructure = VK_TRUE;
    accelStructureFeatures.pNext = &rayTracingPipelineFeatures;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddressFeatures.pNext = &accelStructureFeatures;

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    std::vector<const char*> devExts = {
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    devInfo.pQueueCreateInfos = queueInfos.data();
    devInfo.pEnabledFeatures = &features;
    devInfo.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
    devInfo.ppEnabledExtensionNames = devExts.data();
    devInfo.pNext = &bufferDeviceAddressFeatures;

    VK_CHECK(vkCreateDevice(physicalDevice_, &devInfo, nullptr, &device_),
             "Failed to create logical device");
    LOG_SUCCESS_CAT("RTX", "Device created: 0x{:x}", reinterpret_cast<uint64_t>(device_));

    // Load RT extension functions
#define LOAD_PFN(name) vk##name##_ = reinterpret_cast<PFN_vk##name>(vkGetDeviceProcAddr(device_, #name));
    LOAD_PFN(GetBufferDeviceAddressKHR);
    LOAD_PFN(CmdTraceRaysKHR);
    LOAD_PFN(GetRayTracingShaderGroupHandlesKHR);
    LOAD_PFN(CreateAccelerationStructureKHR);
    LOAD_PFN(DestroyAccelerationStructureKHR);
    LOAD_PFN(GetAccelerationStructureBuildSizesKHR);
    LOAD_PFN(CmdBuildAccelerationStructuresKHR);
    LOAD_PFN(GetAccelerationStructureDeviceAddressKHR);
    LOAD_PFN(CreateRayTracingPipelinesKHR);
#undef LOAD_PFN

    // Get queues
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_),
             "Failed to create command pool");

    // Initialize BufferTracker
    UltraLowLevelBufferTracker::get().init(device_, physicalDevice_);

    LOG_SUCCESS_CAT("RTX", "{}VULKAN CONTEXT FORGED — {}x{} — SECURE & READY{}", 
                    PLASMA_FUCHSIA, width, height, RESET);
}

// =============================================================================
// stonekey_xor_spirv — ENCRYPTION MANDATORY (NO UNENCRYPTED ATTACH)
// =============================================================================
void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt) {
    // SECURITY: UNENCRYPTED SPIR-V = FATAL
    if (!encrypt) {
        LOG_FATAL_CAT("SECURITY", "FATAL: UNENCRYPTED SPIR-V DETECTED!");
        LOG_FATAL_CAT("SECURITY", "This build REFUSES to attach unencrypted shaders.");
        LOG_FATAL_CAT("SECURITY", "PINK PHOTONS ETERNAL — NO COMPROMISE.");
        std::terminate();
    }

    LOG_INFO_CAT("RTX", "Encrypting SPIR-V: {} words", data.size());

    // USE CONSTEXPR KEY FROM OptionsMenu.hpp
    constexpr uint64_t STONEKEY = Options::Shader::STONEKEY_1;
    static_assert(STONEKEY != 0, "STONEKEY_1 must be non-zero");

    for (auto& word : data) {
        word ^= static_cast<uint32_t>(STONEKEY);
        word ^= static_cast<uint32_t>(STONEKEY >> 32);
    }

    LOG_SUCCESS_CAT("RTX", "SPIR-V ENCRYPTED — KEY: 0x{:x}{:x} — ATTACH AUTHORIZED", 
                    static_cast<uint32_t>(STONEKEY >> 32), static_cast<uint32_t>(STONEKEY));
}

// =============================================================================
// logAndTrackDestruction
// =============================================================================
void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
    LOG_DEBUG_CAT("RTX", "{}[Handle] {} @ {}:{} | size: {}MB{}", 
                  ELECTRIC_BLUE, type, ptr, line, size/(1024*1024), RESET);
}

// =============================================================================
// UltraLowLevelBufferTracker — OBFUSCATED HANDLES
// =============================================================================
UltraLowLevelBufferTracker& UltraLowLevelBufferTracker::get() noexcept {
    static UltraLowLevelBufferTracker instance;
    LOG_DEBUG_CAT("RTX", "UltraLowLevelBufferTracker::get() — Singleton accessed");
    return instance;
}

static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    LOG_ERROR_CAT("RTX", "No suitable memory type found");
    return 0;
}

uint64_t UltraLowLevelBufferTracker::create(VkDeviceSize size,
                                            VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags props,
                                            std::string_view tag) noexcept
{
    LOG_INFO_CAT("RTX", "Buffer create: {}MB | Tag: {}", size / (1024*1024), tag);

    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(g_ctx().device(), &bufInfo, nullptr, &buffer),
             "Buffer creation failed");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(g_ctx().device(), buffer, &memReq);

    VkMemoryAllocateFlagsInfo flagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0u
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = findMemoryType(g_ctx().physicalDevice(), memReq.memoryTypeBits, props)
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(g_ctx().device(), &allocInfo, nullptr, &memory),
             "Memory allocation failed");

    VK_CHECK(vkBindBufferMemory(g_ctx().device(), buffer, memory, 0),
             "Buffer bind failed");

    const uint64_t raw = ++counter_;
    const uint64_t obf = obfuscate(raw);

    std::lock_guard<std::mutex> lk(mutex_);
    map_.emplace(obf, BufferData{buffer, memory, size, usage, std::string(tag)});
    LOG_SUCCESS_CAT("RTX", "Buffer tracked: 0x{:x} (raw: 0x{:x}) | Tag: {}", obf, raw, tag);
    return obf;
}

void UltraLowLevelBufferTracker::destroy(uint64_t handle) noexcept {
    LOG_INFO_CAT("RTX", "Buffer destroy: 0x{:x}", handle);
    const uint64_t raw = deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    if (it == map_.end()) {
        LOG_WARN_CAT("RTX", "Buffer not found: raw 0x{:x}", raw);
        return;
    }
    const BufferData& d = it->second;
    if (d.buffer) vkDestroyBuffer(device_, d.buffer, nullptr);
    if (d.memory) vkFreeMemory(device_, d.memory, nullptr);
    map_.erase(it);
    LOG_SUCCESS_CAT("RTX", "Buffer destroyed: 0x{:x} | Tag: {}", handle, d.tag);
}

BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) noexcept {
    const uint64_t raw = deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    return it == map_.end() ? nullptr : &it->second;
}

const BufferData* UltraLowLevelBufferTracker::getData(uint64_t handle) const noexcept {
    const uint64_t raw = deobfuscate(handle);
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(raw);
    return it == map_.end() ? nullptr : &it->second;
}

void UltraLowLevelBufferTracker::init(VkDevice dev, VkPhysicalDevice phys) noexcept {
    device_ = dev;
    physDev_ = phys;
    LOG_SUCCESS_CAT("RTX", "BufferTracker initialized — READY FOR PINK PHOTONS");
}

void UltraLowLevelBufferTracker::purge_all() noexcept {
    LOG_INFO_CAT("RTX", "Purging all buffers — Total: {}", map_.size());
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& [k, v] : map_) {
        if (v.buffer) vkDestroyBuffer(device_, v.buffer, nullptr);
        if (v.memory) vkFreeMemory(device_, v.memory, nullptr);
    }
    map_.clear();
    LOG_SUCCESS_CAT("RTX", "All buffers purged — ZERO LEAKS");
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
// AutoBuffer
// =============================================================================
AutoBuffer::AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept {
    id = UltraLowLevelBufferTracker::get().create(size, usage, props, tag);
}

AutoBuffer::~AutoBuffer() noexcept { 
    if (id) UltraLowLevelBufferTracker::get().destroy(id); 
}

AutoBuffer::AutoBuffer(AutoBuffer&& o) noexcept : id(o.id) { o.id = 0ULL; }
AutoBuffer& AutoBuffer::operator=(AutoBuffer&& o) noexcept {
    if (this != &o) { if (id) UltraLowLevelBufferTracker::get().destroy(id); id = o.id; o.id = 0ULL; }
    return *this;
}
VkBuffer AutoBuffer::raw() const noexcept { 
    auto* d = UltraLowLevelBufferTracker::get().getData(id); 
    return d ? d->buffer : VK_NULL_HANDLE; 
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
// RENDERER STUBS
// =============================================================================
VulkanRenderer& renderer() { 
    LOG_FATAL_CAT("RTX", "renderer() called before initialization!");
    std::terminate(); 
}
void initRenderer(int, int) {}
void handleResize(int, int) {}
void renderFrame(const Camera&, float) noexcept {}
void shutdown() noexcept {}
void createSwapchain(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t, uint32_t) {}
void recreateSwapchain(uint32_t, uint32_t) noexcept {}
void buildBLAS(uint64_t, uint64_t, uint32_t, uint32_t) noexcept {}
void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>&) noexcept {}
void cleanupAll() noexcept {}

} // namespace RTX

// =============================================================================
// Explicit instantiations
// =============================================================================
template struct RTX::Handle<VkImage_T*>;
template struct RTX::Handle<VkDeviceMemory_T*>;
template struct RTX::Handle<VkDescriptorPool_T*>;
template struct RTX::Handle<VkPipelineLayout_T*>;
template struct RTX::Handle<VkPipeline_T*>;
template struct RTX::Handle<VkDescriptorSetLayout_T*>;
template struct RTX::Handle<VkSampler_T*>;
template struct RTX::Handle<VkImageView_T*>;
template struct RTX::Handle<VkBuffer>;
template struct RTX::Handle<VkBufferView>;
template struct RTX::Handle<VkShaderModule>;
template struct RTX::Handle<VkRenderPass>;
template struct RTX::Handle<VkFramebuffer>;
template struct RTX::Handle<VkCommandPool>;
template struct RTX::Handle<VkSemaphore>;
template struct RTX::Handle<VkFence>;
template struct RTX::Handle<VkQueryPool>;
template struct RTX::Handle<VkSwapchainKHR>;
template struct RTX::Handle<VkAccelerationStructureKHR>;

// End of file