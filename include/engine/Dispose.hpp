// include/engine/Dispose.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <type_traits>

namespace Vulkan { struct Context; }

namespace Dispose {

template<typename T>
class VulkanHandle {
public:
    using DestroyFunc = void(*)(VkDevice, T, const VkAllocationCallbacks*);
    using DestroyLambda = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    VulkanHandle() = default;

    // Standard constructor (function pointer)
    VulkanHandle(VkDevice device, T handle, DestroyFunc destroy, const std::string& name = "")
        : device_(device), handle_(handle), destroy_(destroy), name_(name) {}

    // NEW: Lambda constructor
    VulkanHandle(VkDevice device, T handle, DestroyLambda destroy, const std::string& name = "")
        : device_(device), handle_(handle), name_(name) {
        if (destroy) {
            destroy_ = [destroy = std::move(destroy)](VkDevice d, T h, const VkAllocationCallbacks* a) {
                destroy(d, h, a);
            };
        }
    }

    // nullptr constructor
    VulkanHandle(VkDevice device, std::nullptr_t, const std::string& name = "")
        : device_(device), handle_(VK_NULL_HANDLE), destroy_(nullptr), name_(name) {}

    // move
    VulkanHandle(VulkanHandle&& o) noexcept
        : device_(o.device_), handle_(o.handle_), destroy_(std::move(o.destroy_)), name_(std::move(o.name_)) {
        o.handle_ = VK_NULL_HANDLE; o.destroy_ = nullptr;
    }
    VulkanHandle& operator=(VulkanHandle&& o) noexcept {
        if (this != &o) {
            reset();
            device_ = o.device_;
            handle_ = o.handle_;
            destroy_ = std::move(o.destroy_);
            name_ = std::move(o.name_);
            o.handle_ = VK_NULL_HANDLE;
            o.destroy_ = nullptr;
        }
        return *this;
    }

    // delete copy
    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    ~VulkanHandle() { reset(); }

    T get() const { return handle_; }
    operator T() const { return handle_; }

    T* raw() { return &handle_; }
    const T* raw() const { return &handle_; }

    void reset() {
        if (handle_ != VK_NULL_HANDLE && destroy_) {
            destroy_(device_, handle_, nullptr);
        }
        handle_ = VK_NULL_HANDLE;
        destroy_ = nullptr;
    }

    void setDestroyFunc(DestroyFunc f) { destroy_ = f; }

    // ---- static destroy lookup (function pointer only) ----
    static DestroyFunc getDestroyFunc(VkDevice) {
        if constexpr (std::is_same_v<T, VkBuffer>)               return vkDestroyBuffer;
        else if constexpr (std::is_same_v<T, VkDeviceMemory>)    return vkFreeMemory;
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return vkDestroyDescriptorSetLayout;
        else if constexpr (std::is_same_v<T, VkDescriptorPool>)  return vkDestroyDescriptorPool;
        else if constexpr (std::is_same_v<T, VkPipelineLayout>)  return vkDestroyPipelineLayout;
        else if constexpr (std::is_same_v<T, VkPipeline>)        return vkDestroyPipeline;
        else if constexpr (std::is_same_v<T, VkRenderPass>)      return vkDestroyRenderPass;
        else if constexpr (std::is_same_v<T, VkImage>)           return vkDestroyImage;
        else if constexpr (std::is_same_v<T, VkImageView>)       return vkDestroyImageView;
        else if constexpr (std::is_same_v<T, VkPipelineCache>)   return vkDestroyPipelineCache;
        else return nullptr;
    }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    T handle_ = VK_NULL_HANDLE;
    DestroyLambda destroy_;
    std::string name_;
};

// ---------------------------------------------------------------------
// makeHandle – factory (inline, no .cpp needed)
// ---------------------------------------------------------------------
template<typename T>
[[nodiscard]] inline VulkanHandle<T> makeHandle(VkDevice device, T handle, const std::string& name = "") {
    auto destroy = VulkanHandle<T>::getDestroyFunc(device);
    return VulkanHandle<T>(device, handle, destroy, name);
}

template<typename T>
[[nodiscard]] inline VulkanHandle<T> makeHandle(VkDevice device, std::nullptr_t, const std::string& name = "") {
    return VulkanHandle<T>(device, nullptr, name);
}

// ---------------------------------------------------------------------
// SDL cleanup
// ---------------------------------------------------------------------
inline void quitSDL() noexcept { SDL_Quit(); }
inline void destroyWindow(SDL_Window* w) { if (w) SDL_DestroyWindow(w); }
struct SDLWindowDeleter { void operator()(SDL_Window* w) const { destroyWindow(w); } };

// ---------------------------------------------------------------------
// Central Vulkan cleanup
// ---------------------------------------------------------------------
void cleanupAll(Vulkan::Context& ctx) noexcept;

} // namespace Dispose