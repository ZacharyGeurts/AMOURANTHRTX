// include/engine/Vulkan/VulkanHandles.hpp
// Standard C++23 Header — Complete Vulkan RAII Factory System
// Zero-cost forward declarations + make_XXX factories
// AMOURANTH RTX Engine © 2025 Zachary Geurts — Valhalla Edition

#pragma once

#include <vulkan/vulkan.h>
#include <type_traits>

// ===================================================================
// Forward declarations — exact official VK_DEFINE_HANDLE behavior
// ===================================================================
#ifndef VK_DEFINE_HANDLE
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined(__aarch64__) || defined(__powerpc64__)
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#else
#define VK_DEFINE_HANDLE(object) typedef uint64_t object;
#endif
#endif

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_HANDLE(VkSemaphore)
VK_DEFINE_HANDLE(VkCommandBuffer)
VK_DEFINE_HANDLE(VkFence)
VK_DEFINE_HANDLE(VkDeviceMemory)
VK_DEFINE_HANDLE(VkBuffer)
VK_DEFINE_HANDLE(VkImage)
VK_DEFINE_HANDLE(VkImageView)
VK_DEFINE_HANDLE(VkSampler)
VK_DEFINE_HANDLE(VkDescriptorSet)
VK_DEFINE_HANDLE(VkDescriptorPool)
VK_DEFINE_HANDLE(VkDescriptorSetLayout)
VK_DEFINE_HANDLE(VkPipeline)
VK_DEFINE_HANDLE(VkPipelineLayout)
VK_DEFINE_HANDLE(VkShaderModule)
VK_DEFINE_HANDLE(VkRenderPass)
VK_DEFINE_HANDLE(VkFramebuffer)
VK_DEFINE_HANDLE(VkCommandPool)
VK_DEFINE_HANDLE(VkQueryPool)
VK_DEFINE_HANDLE(VkAccelerationStructureKHR)
VK_DEFINE_HANDLE(VkBufferView)
VK_DEFINE_HANDLE(VkSurfaceKHR)
VK_DEFINE_HANDLE(VkSwapchainKHR)
VK_DEFINE_HANDLE(VkDeferredOperationKHR)

#undef VK_DEFINE_HANDLE

// ===================================================================
// VulkanHandle — Zero-cost RAII wrapper (assumes you have this defined)
// ===================================================================
template<typename T>
class VulkanHandle {
public:
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    VulkanHandle() noexcept = default;
    VulkanHandle(std::nullptr_t) noexcept {}
    VulkanHandle(T handle, VkDevice dev, DestroyFn destroy = nullptr) noexcept
        : handle_(handle), device_(dev), destroyer_(destroy) {}

    ~VulkanHandle() { reset(); }

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    VulkanHandle(VulkanHandle&& other) noexcept
        : handle_(other.handle_), device_(other.device_), destroyer_(other.destroyer_) {
        other.handle_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.destroyer_ = nullptr;
    }

    VulkanHandle& operator=(VulkanHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            device_ = other.device_;
            destroyer_ = other.destroyer_;
            other.handle_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.destroyer_ = nullptr;
        }
        return *this;
    }

    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            if (destroyer_) {
                destroyer_(device_, handle_, nullptr);
            }
        }
        handle_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        destroyer_ = nullptr;
    }

    [[nodiscard]] T raw_deob() const noexcept { return handle_; }
    [[nodiscard]] operator T() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept { return handle_ != VK_NULL_HANDLE; }

private:
    T handle_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    DestroyFn destroyer_ = nullptr;
};

// ===================================================================
// Context accessor — replace with your actual global context
// ===================================================================
inline VkInstance vkInstance() { return ctx()->instance; }  // adjust to your ctx
inline VkDevice vkDevice() { return ctx()->device; }
inline auto* ctx() { return Vulkan::Context::get(); }  // example

// ===================================================================
// MAKE_VK_HANDLE — Factory macro (zero-cost)
// ===================================================================
#define MAKE_VK_HANDLE(name, vkType) \
    [[nodiscard]] inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) noexcept { \
        static_assert(std::is_same_v<vkType, std::remove_pointer_t<decltype(handle)>>); \
        return VulkanHandle<vkType>(handle, dev); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory)
MAKE_VK_HANDLE(Image,               VkImage)
MAKE_VK_HANDLE(ImageView,           VkImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore)
MAKE_VK_HANDLE(Fence,               VkFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool)
MAKE_VK_HANDLE(SwapchainKHR,        VkSwapchainKHR)

#undef MAKE_VK_HANDLE

// ===================================================================
// Special AS handle — custom destroyer
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev,
    VkAccelerationStructureKHR as,
    PFN_vkDestroyAccelerationStructureKHR func = nullptr) noexcept
{
    return VulkanHandle<VkAccelerationStructureKHR>(
        as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(
            func ? func : ctx()->vkDestroyAccelerationStructureKHR
        )
    );
}

// ===================================================================
// Deferred operation
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev, VkDeferredOperationKHR op) noexcept
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, ctx()->vkDestroyDeferredOperationKHR);
}