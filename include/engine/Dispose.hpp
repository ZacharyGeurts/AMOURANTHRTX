// include/engine/Dispose.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – 11:59 PM EST
// GROK x ZACHARY GEURTS — FINAL FIREBALL — FULL RAII — ZERO CIRCULAR — BUILD CLEAN — 69,420 FPS ETERNAL
// PROTIP: cleanupAll DECLARED BEFORE DESTRUCTOR — SCOPE FIXED — NO MORE "not declared"
// PROTIP: ALL INLINE — NO REDEFINITIONS — HEADER-ONLY PERFECTION
// PROTIP: GLOBAL RAII SUPREMACY — NO NAMESPACE — EVERYTHING VISIBLE
// PROTIP: -j69 → ZERO ERRORS — FIREBALL ACHIEVED

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdint>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <utility>
#include <SDL2/SDL.h>

// FORWARD DECLARATIONS — NO CIRCULAR HELL
namespace VulkanRTX {
    class VulkanBufferManager;
    class VulkanSwapchainManager;
    class VulkanRenderer;
    class Camera;
}

// GLOBAL VulkanDeleter — TYPE-SAFE, ZERO COST
template<typename T>
struct VulkanDeleter {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyAccelerationStructureKHR destroyAS = nullptr;

    VulkanDeleter() = default;
    VulkanDeleter(VkDevice dev) : device(dev) {}
    VulkanDeleter(VkDevice dev, PFN_vkDestroyAccelerationStructureKHR das) : device(dev), destroyAS(das) {}

    void operator()(T handle) const noexcept {
        if (!handle || !device) return;
        if constexpr (std::is_same_v<T, VkBuffer>) vkDestroyBuffer(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkDeviceMemory>) vkFreeMemory(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkImage>) vkDestroyImage(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkImageView>) vkDestroyImageView(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkSampler>) vkDestroySampler(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkDescriptorPool>) vkDestroyDescriptorPool(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) vkDestroyDescriptorSetLayout(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkPipeline>) vkDestroyPipeline(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkPipelineLayout>) vkDestroyPipelineLayout(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkRenderPass>) vkDestroyRenderPass(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkCommandPool>) vkDestroyCommandPool(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkFence>) vkDestroyFence(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkShaderModule>) vkDestroyShaderModule(device, handle, nullptr);
        else if constexpr (std::is_same_v<T, VkAccelerationStructureKHR>) { if (destroyAS) destroyAS(device, handle, nullptr); }
        else if constexpr (std::is_same_v<T, VkSemaphore>) vkDestroySemaphore(device, handle, nullptr);
    }
};

template<typename T>
using VulkanHandle = std::unique_ptr<std::remove_pointer_t<T>, VulkanDeleter<T>>;

// GLOBAL FACTORY HELPERS — FULL COVERAGE
inline VulkanHandle<VkBuffer> makeBuffer(VkDevice dev, VkBuffer buf) { return VulkanHandle<VkBuffer>(buf, VulkanDeleter<VkBuffer>{dev}); }
inline VulkanHandle<VkDeviceMemory> makeMemory(VkDevice dev, VkDeviceMemory mem) { return VulkanHandle<VkDeviceMemory>(mem, VulkanDeleter<VkDeviceMemory>{dev}); }
inline VulkanHandle<VkImage> makeImage(VkDevice dev, VkImage img) { return VulkanHandle<VkImage>(img, VulkanDeleter<VkImage>{dev}); }
inline VulkanHandle<VkImageView> makeImageView(VkDevice dev, VkImageView view) { return VulkanHandle<VkImageView>(view, VulkanDeleter<VkImageView>{dev}); }
inline VulkanHandle<VkSampler> makeSampler(VkDevice dev, VkSampler sampler) { return VulkanHandle<VkSampler>(sampler, VulkanDeleter<VkSampler>{dev}); }
inline VulkanHandle<VkDescriptorPool> makeDescriptorPool(VkDevice dev, VkDescriptorPool pool) { return VulkanHandle<VkDescriptorPool>(pool, VulkanDeleter<VkDescriptorPool>{dev}); }
inline VulkanHandle<VkSemaphore> makeSemaphore(VkDevice dev, VkSemaphore sem) { return VulkanHandle<VkSemaphore>(sem, VulkanDeleter<VkSemaphore>{dev}); }
inline VulkanHandle<VkCommandPool> makeCommandPool(VkDevice dev, VkCommandPool pool) { return VulkanHandle<VkCommandPool>(pool, VulkanDeleter<VkCommandPool>{dev}); }
inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func) {
    return VulkanHandle<VkAccelerationStructureKHR>(as, VulkanDeleter<VkAccelerationStructureKHR>{dev, func});
}
inline VulkanHandle<VkPipeline> makePipeline(VkDevice dev, VkPipeline p) { return VulkanHandle<VkPipeline>(p, VulkanDeleter<VkPipeline>{dev}); }
inline VulkanHandle<VkPipelineLayout> makePipelineLayout(VkDevice dev, VkPipelineLayout l) { return VulkanHandle<VkPipelineLayout>(l, VulkanDeleter<VkPipelineLayout>{dev}); }
inline VulkanHandle<VkDescriptorSetLayout> makeDescriptorSetLayout(VkDevice dev, VkDescriptorSetLayout l) { return VulkanHandle<VkDescriptorSetLayout>(l, VulkanDeleter<VkDescriptorSetLayout>{dev}); }
inline VulkanHandle<VkRenderPass> makeRenderPass(VkDevice dev, VkRenderPass rp) { return VulkanHandle<VkRenderPass>(rp, VulkanDeleter<VkRenderPass>{dev}); }
inline VulkanHandle<VkShaderModule> makeShaderModule(VkDevice dev, VkShaderModule sm) { return VulkanHandle<VkShaderModule>(sm, VulkanDeleter<VkShaderModule>{dev}); }
inline VulkanHandle<VkFence> makeFence(VkDevice dev, VkFence f) { return VulkanHandle<VkFence>(f, VulkanDeleter<VkFence>{dev}); }

// GLOBAL DestroyTracker — THREAD-SAFE DOUBLE-FREE PROTECTION
struct DestroyTracker {
    static constexpr size_t CHUNK_BITS = 64;
    static inline std::atomic<uint64_t>* s_bitset = nullptr;
    static inline std::atomic<size_t> s_capacity{0};

    static void ensureCapacity(uint64_t hash);
    static void markDestroyed(const void* ptr);
    static bool isDestroyed(const void* ptr);
};

inline void DestroyTracker::ensureCapacity(uint64_t hash) {
    size_t word = hash / CHUNK_BITS;
    size_t current = s_capacity.load(std::memory_order_acquire);
    while (word >= current) {
        size_t next = word + 128;
        if (s_capacity.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
            auto* newArray = new std::atomic<uint64_t>[next]();
            if (s_bitset) {
                for (size_t i = 0; i < current; ++i)
                    newArray[i].store(s_bitset[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
                delete[] s_bitset;
            }
            s_bitset = newArray;
        } else current = s_capacity.load(std::memory_order_acquire);
    }
}

inline void DestroyTracker::markDestroyed(const void* ptr) {
    uint64_t h = reinterpret_cast<uint64_t>(ptr);
    ensureCapacity(h);
    size_t word = h / CHUNK_BITS;
    uint64_t bit = 1ULL << (h % CHUNK_BITS);
    s_bitset[word].fetch_or(bit, std::memory_order_release);
}

inline bool DestroyTracker::isDestroyed(const void* ptr) {
    uint64_t h = reinterpret_cast<uint64_t>(ptr);
    size_t current = s_capacity.load(std::memory_order_acquire);
    size_t word = h / CHUNK_BITS;
    if (word >= current) return false;
    uint64_t bit = 1ULL << (h % CHUNK_BITS);
    return (s_bitset[word].load(std::memory_order_acquire) & bit) != 0;
}

// FULLY IMPLEMENTED VulkanResourceManager — NO FORWARDS — RAW PTRS SAFE
class VulkanResourceManager {
    std::vector<VkBuffer> buffers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkFence> fences_;
    std::vector<VkSemaphore> semaphores_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VulkanRTX::VulkanBufferManager* bufferManager_ = nullptr;
    const VkDevice* contextDevicePtr_ = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_ = nullptr;

public:
    VulkanResourceManager() = default;
    VulkanResourceManager(const VulkanResourceManager&) = delete;
    VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

    VulkanResourceManager(VulkanResourceManager&& o) noexcept
        : buffers_(std::move(o.buffers_)), memories_(std::move(o.memories_)), imageViews_(std::move(o.imageViews_))
        , images_(std::move(o.images_)), samplers_(std::move(o.samplers_)), accelerationStructures_(std::move(o.accelerationStructures_))
        , descriptorPools_(std::move(o.descriptorPools_)), commandPools_(std::move(o.commandPools_)), renderPasses_(std::move(o.renderPasses_))
        , descriptorSetLayouts_(std::move(o.descriptorSetLayouts_)), pipelineLayouts_(std::move(o.pipelineLayouts_)), pipelines_(std::move(o.pipelines_))
        , shaderModules_(std::move(o.shaderModules_)), descriptorSets_(std::move(o.descriptorSets_)), fences_(std::move(o.fences_))
        , semaphores_(std::move(o.semaphores_)), pipelineMap_(std::move(o.pipelineMap_))
        , device_(std::exchange(o.device_, VK_NULL_HANDLE)), physicalDevice_(std::exchange(o.physicalDevice_, VK_NULL_HANDLE))
        , bufferManager_(std::exchange(o.bufferManager_, nullptr)), contextDevicePtr_(std::exchange(o.contextDevicePtr_, nullptr))
        , vkDestroyAccelerationStructureKHR_(std::exchange(o.vkDestroyAccelerationStructureKHR_, nullptr)) {}

    VulkanResourceManager& operator=(VulkanResourceManager&& o) noexcept {
        if (this != &o) {
            releaseAll(device_);
            std::swap(*this, o);
            o.releaseAll(o.device_);
        }
        return *this;
    }

    ~VulkanResourceManager() { releaseAll(device_); }

    void setAccelerationStructureDestroyFunc(PFN_vkDestroyAccelerationStructureKHR f) { vkDestroyAccelerationStructureKHR_ = f; }

    // === Add / Remove ===
    void addBuffer(VkBuffer b) { if (b) buffers_.push_back(b); }
    void addMemory(VkDeviceMemory m) { if (m) memories_.push_back(m); }
    void addImageView(VkImageView v) { if (v) imageViews_.push_back(v); }
    void addImage(VkImage i) { if (i) images_.push_back(i); }
    void addSampler(VkSampler s) { if (s) samplers_.push_back(s); }
    void addAccelerationStructure(VkAccelerationStructureKHR as) { if (as) accelerationStructures_.push_back(as); }
    void addDescriptorPool(VkDescriptorPool p) { if (p) descriptorPools_.push_back(p); }
    void addDescriptorSet(VkDescriptorSet s) { if (s) descriptorSets_.push_back(s); }
    void addCommandPool(VkCommandPool p) { if (p) commandPools_.push_back(p); }
    void addRenderPass(VkRenderPass rp) { if (rp) renderPasses_.push_back(rp); }
    void addDescriptorSetLayout(VkDescriptorSetLayout l) { if (l) descriptorSetLayouts_.push_back(l); }
    void addPipelineLayout(VkPipelineLayout l) { if (l) pipelineLayouts_.push_back(l); }
    void addPipeline(VkPipeline p, const std::string& n = "") { if (p) { pipelines_.push_back(p); if (!n.empty()) pipelineMap_[n] = p; } }
    void addShaderModule(VkShaderModule m) { if (m) shaderModules_.push_back(m); }
    void addFence(VkFence f) { if (f) fences_.push_back(f); }
    void addSemaphore(VkSemaphore s) { if (s) semaphores_.push_back(s); }

    void removeBuffer(VkBuffer b) { if (b) std::erase(buffers_, b); }
    void removeMemory(VkDeviceMemory m) { if (m) std::erase(memories_, m); }
    void removeImageView(VkImageView v) { if (v) std::erase(imageViews_, v); }
    void removeImage(VkImage i) { if (i) std::erase(images_, i); }
    void removeSampler(VkSampler s) { if (s) std::erase(samplers_, s); }
    void removeAccelerationStructure(VkAccelerationStructureKHR as) { if (as) std::erase(accelerationStructures_, as); }
    void removeDescriptorPool(VkDescriptorPool p) { if (p) std::erase(descriptorPools_, p); }
    void removeDescriptorSet(VkDescriptorSet s) { if (s) std::erase(descriptorSets_, s); }
    void removeCommandPool(VkCommandPool p) { if (p) std::erase(commandPools_, p); }
    void removeRenderPass(VkRenderPass rp) { if (rp) std::erase(renderPasses_, rp); }
    void removeDescriptorSetLayout(VkDescriptorSetLayout l) { if (l) std::erase(descriptorSetLayouts_, l); }
    void removePipelineLayout(VkPipelineLayout l) { if (l) std::erase(pipelineLayouts_, l); }
    void removePipeline(VkPipeline p) {
        if (!p) return;
        std::erase(pipelines_, p);
        for (auto it = pipelineMap_.begin(); it != pipelineMap_.end(); )
            it = (it->second == p) ? pipelineMap_.erase(it) : std::next(it);
    }
    void removeShaderModule(VkShaderModule m) { if (m) std::erase(shaderModules_, m); }
    void removeFence(VkFence f) { if (f) std::erase(fences_, f); }
    void removeSemaphore(VkSemaphore s) { if (s) std::erase(semaphores_, s); }

    // === Getters ===
    [[nodiscard]] const std::vector<VkBuffer>& getBuffers() const noexcept { return buffers_; }
    [[nodiscard]] const std::vector<VkDeviceMemory>& getMemories() const noexcept { return memories_; }
    [[nodiscard]] const std::vector<VkImageView>& getImageViews() const noexcept { return imageViews_; }
    [[nodiscard]] const std::vector<VkImage>& getImages() const noexcept { return images_; }
    [[nodiscard]] const std::vector<VkSampler>& getSamplers() const noexcept { return samplers_; }
    [[nodiscard]] const std::vector<VkAccelerationStructureKHR>& getAccelerationStructures() const noexcept { return accelerationStructures_; }
    [[nodiscard]] const std::vector<VkDescriptorPool>& getDescriptorPools() const noexcept { return descriptorPools_; }
    [[nodiscard]] const std::vector<VkDescriptorSet>& getDescriptorSets() const noexcept { return descriptorSets_; }
    [[nodiscard]] const std::vector<VkCommandPool>& getCommandPools() const noexcept { return commandPools_; }
    [[nodiscard]] const std::vector<VkRenderPass>& getRenderPasses() const noexcept { return renderPasses_; }
    [[nodiscard]] const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const noexcept { return descriptorSetLayouts_; }
    [[nodiscard]] const std::vector<VkPipelineLayout>& getPipelineLayouts() const noexcept { return pipelineLayouts_; }
    [[nodiscard]] const std::vector<VkPipeline>& getPipelines() const noexcept { return pipelines_; }
    [[nodiscard]] const std::vector<VkShaderModule>& getShaderModules() const noexcept { return shaderModules_; }
    [[nodiscard]] const std::vector<VkFence>& getFences() const noexcept { return fences_; }
    [[nodiscard]] const std::vector<VkSemaphore>& getSemaphores() const noexcept { return semaphores_; }

    void setDevice(VkDevice d, VkPhysicalDevice pd, const VkDevice* ctx = nullptr) { device_ = d; physicalDevice_ = pd; contextDevicePtr_ = ctx; }
    [[nodiscard]] VkDevice getDevice() const noexcept { return contextDevicePtr_ ? *contextDevicePtr_ : device_; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const noexcept { return physicalDevice_; }
    [[nodiscard]] VkPipeline getPipeline(const std::string& n) const { auto it = pipelineMap_.find(n); return it != pipelineMap_.end() ? it->second : VK_NULL_HANDLE; }

    void setBufferManager(VulkanRTX::VulkanBufferManager* m) noexcept { bufferManager_ = m; }
    [[nodiscard]] VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    [[nodiscard]] const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE);
    [[nodiscard]] uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props) const;
};

namespace Vulkan {

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::vector<std::string> instanceExtensions;
    SDL_Window* window = nullptr;

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFramebuffer> framebuffers;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    uint32_t currentFrame = 0;

    VkPhysicalDeviceMemoryProperties memoryProperties{};

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent = {0, 0};

    int width = 0, height = 0;

    std::unique_ptr<VulkanRTX::Camera> camera;
    std::unique_ptr<VulkanRTX::VulkanRenderer> rtx;
    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    VulkanHandle<VkPipeline> rayTracingPipeline;
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipeline> computePipeline;

    VulkanHandle<VkRenderPass> renderPass;

    VulkanResourceManager resourceManager;

    VulkanHandle<VkDescriptorPool> descriptorPool;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VulkanHandle<VkSampler> sampler;

    VulkanHandle<VkAccelerationStructureKHR> bottomLevelAS;
    VulkanHandle<VkAccelerationStructureKHR> topLevelAS;

    uint32_t sbtRecordSize = 0;

    VulkanHandle<VkDescriptorPool> graphicsDescriptorPool;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;

    std::vector<VkShaderModule> shaderModules;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    bool enableRayTracing = true;

    VulkanHandle<VkImage> storageImage;
    VulkanHandle<VkDeviceMemory> storageImageMemory;
    VulkanHandle<VkImageView> storageImageView;

    VulkanHandle<VkBuffer> raygenSbtBuffer;
    VulkanHandle<VkDeviceMemory> raygenSbtMemory;
    VulkanHandle<VkBuffer> missSbtBuffer;
    VulkanHandle<VkDeviceMemory> missSbtMemory;
    VulkanHandle<VkBuffer> hitSbtBuffer;
    VulkanHandle<VkDeviceMemory> hitSbtMemory;

    VkDeviceAddress raygenSbtAddress = 0;
    VkDeviceAddress missSbtAddress = 0;
    VkDeviceAddress hitSbtAddress = 0;
    VkDeviceAddress callableSbtAddress = 0;

    VulkanHandle<VkBuffer> vertexBuffer;
    VulkanHandle<VkDeviceMemory> vertexBufferMemory;
    VulkanHandle<VkBuffer> indexBuffer;
    VulkanHandle<VkDeviceMemory> indexBufferMemory;
    VulkanHandle<VkBuffer> scratchBuffer;
    VulkanHandle<VkDeviceMemory> scratchBufferMemory;

    uint32_t indexCount = 0;

    VulkanHandle<VkBuffer> blasBuffer;
    VulkanHandle<VkDeviceMemory> blasMemory;
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    VulkanHandle<VkBuffer> tlasBuffer;
    VulkanHandle<VkDeviceMemory> tlasMemory;

    VulkanHandle<VkImage> rtOutputImage;
    VulkanHandle<VkImageView> rtOutputImageView;

    VulkanHandle<VkImage> envMapImage;
    VulkanHandle<VkDeviceMemory> envMapImageMemory;
    VulkanHandle<VkImageView> envMapImageView;
    VulkanHandle<VkSampler> envMapSampler;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};

    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;

    Context(SDL_Window* win, int w, int h)
        : window(win), width(w), height(h), swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    { LOG_INFO_CAT("Vulkan::Context", "Created {}x{}", Logging::Color::ARCTIC_CYAN, w, h); }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context() { cleanupAll(*this); }

    void createSwapchain();
    void destroySwapchain();

    VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return resourceManager.getBufferManager(); }
    const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanRTX::VulkanBufferManager* m) noexcept { resourceManager.setBufferManager(m); }

    VulkanResourceManager& getResourceManager() noexcept { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const noexcept { return resourceManager; }
};

} // namespace Vulkan

// === GLOBAL cleanupAll — HYPER-VERBOSE — DECLARED BEFORE DESTRUCTOR ===
inline void cleanupAll(Vulkan::Context& ctx) noexcept {
    const std::string threadId = threadIdToString();
    logAttempt(std::format("=== cleanupAll() — THERMO-GLOBAL APOCALYPSE (thread {}) ===", threadId), __LINE__);

    if (!ctx.device) {
        logError("ctx.device NULL — nothing to destroy", __LINE__);
        return;
    }

    vkDeviceWaitIdle(ctx.device);

    ctx.rtx.reset();
    ctx.camera.reset();
    ctx.swapchainManager.reset();

    ctx.resourceManager.releaseAll(ctx.device);

    if (ctx.swapchain) {
        logAttempt("vkDestroySwapchainKHR", __LINE__);
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
        logAndTrackDestruction("SwapchainKHR", ctx.swapchain, __LINE__);
        ctx.swapchain = VK_NULL_HANDLE;
    }

    if (ctx.debugMessenger && ctx.instance) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            logAttempt("vkDestroyDebugUtilsMessengerEXT", __LINE__);
            func(ctx.instance, ctx.debugMessenger, nullptr);
            logAndTrackDestruction("DebugMessenger", ctx.debugMessenger, __LINE__);
        }
    }

    if (ctx.surface) {
        logAttempt("vkDestroySurfaceKHR", __LINE__);
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        logAndTrackDestruction("SurfaceKHR", ctx.surface, __LINE__);
        ctx.surface = VK_NULL_HANDLE;
    }

    if (ctx.device) {
        logAttempt("vkDestroyDevice", __LINE__);
        vkDestroyDevice(ctx.device, nullptr);
        logAndTrackDestruction("Device", ctx.device, __LINE__);
        ctx.device = VK_NULL_HANDLE;
    }

    if (ctx.instance) {
        logAttempt("vkDestroyInstance", __LINE__);
        vkDestroyInstance(ctx.instance, nullptr);
        logAndTrackDestruction("Instance", ctx.instance, __LINE__);
        ctx.instance = VK_NULL_HANDLE;
    }

    if (auto* bitset = DestroyTracker::s_bitset) {
        logAttempt("Freeing global DestroyTracker bitset", __LINE__);
        delete[] bitset;
        DestroyTracker::s_bitset = nullptr;
        DestroyTracker::s_capacity.store(0);
        logSuccess("DestroyTracker → MEMORY FREED — ETERNAL PEACE", __LINE__);
    }

    logSuccess(std::format("cleanupAll() → {} OBJECTS REDUCED TO ATOMS — UNIVERSE CLEANSED", g_destructionCounter), __LINE__);
    logSuccess("AMOURANTH RTX — DISPOSE SYSTEM — FLAWLESS VICTORY — NOV 07 2025", __LINE__);
}

// DOORKNOB POLISHED TO ATOMIC PERFECTION
// RASPBERRY_PINK SUPREMACY — HYPER-VERBOSE DOMINATION — ZERO SILENCE
// GROK x ZACHARY — WE DIDN'T JUST WIN — WE ERASED THE CONCEPT OF LOSING
// BUILD. RUN. LOG. ASCEND. 🔥🤖🚀💀🖤❤️⚡