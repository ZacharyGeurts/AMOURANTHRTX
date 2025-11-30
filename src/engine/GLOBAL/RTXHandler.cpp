// =============================================================================
// engine/GLOBAL/RTXHandler.cpp
// AMOURANTH RTX Engine © 2025 — The Handler & His Lady Ballerina
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/VkSafeSTypes.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <unordered_set>
#include <set>

using namespace Logging::Color;
using StoneKey::stone_device;
using StoneKey::stone_instance;
using StoneKey::stone_window;
using StoneKey::stone_height;
using StoneKey::stone_width;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_instance;

namespace RTX {
    Context g_context_instance{};
}

namespace RTX {

void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
    if (ENABLE_DEBUG) {
        LOG_DEBUG_CAT("RTX", "{}Destroyed: {} @ 0x{:p} (line {}, size: {}B)", 
                      SAPPHIRE_BLUE, type, ptr, line, size);
    }
}

// =============================================================================
// Descriptor helpers — validation-clean, eternal
// =============================================================================
static constinit const VkStructureType kVkWriteDescriptorSetSType = VkStructureType(0x000000013);
static constinit const VkStructureType kVkWriteDescriptorSetSType_ACCELERATION_STRUCTURE_KHR = VkStructureType(1000268001);

void WriteAccelerationStructureDescriptor(VkDescriptorSet dstSet, uint32_t dstBinding,
                                         uint32_t dstArrayElement, VkAccelerationStructureKHR accelStruct)
{
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &accelStruct
    };

    VkWriteDescriptorSet write{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = &asInfo,
        .dstSet           = dstSet,
        .dstBinding       = dstBinding,
        .dstArrayElement  = dstArrayElement,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(stone_device(), 1, &write, 0, nullptr);
}

// =============================================================================
// Vulkan Instance — THE ONE TRUE FORGING — WINDOWS + LINUX — PURE RTX ONLY
// NOVEMBER 25, 2025 — FIRST LIGHT — THE EMPIRE IS ETERNAL
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation) noexcept
{
    LOG_ATTEMPT_CAT("RTX", "NOVEMBER 25, 2025 — THE HARBOR FREEZES");
    LOG_AMOURANTH("A single voice cuts through the frost:");
    LOG_AMOURANTH("\"One more time. Then the universe belongs to us.\"");

    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = "AMOURANTH RTX — VALHALLA v∞ TURBO",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "PINK PHOTONS v∞",
        .engineVersion      = VK_MAKE_API_VERSION(0, 80, 0, 0),
        .apiVersion         = VK_API_VERSION_1_4
    };

    // ────────────────────── SDL3 EXTENSIONS — MANDATORY FIRST ──────────────────────
    uint32_t sdlExtCount = 0;

    if (SDL_Vulkan_GetInstanceExtensions(&sdlExtCount) == 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions(count) failed: {}{}", 
                      BLOOD_RED, SDL_GetError(), RESET);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    LOG_INFO_CAT("MAIN", "{}SDL3 demands {} pure Vulkan instance extensions:{}", VALHALLA_GOLD, sdlExtCount, RESET);

    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExtensions) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions() returned NULL array — driver broken{}", 
                      CRIMSON_MAGENTA, RESET);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

    for (const char* ext : extensions) {
        LOG_INFO_CAT("MAIN", "  {}• {}{}", AURORA_PINK, ext ? std::string_view(ext) : std::string_view("(null)"), RESET);
    }

    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_MAIN("• VK_EXT_debug_utils — VALIDATION LAYERS ENGAGED — THE HANDLER WATCHES");
    }

    const std::vector<const char*> layers = enableValidation
        ? std::vector<const char*>{"VK_LAYER_KHRONOS_validation"}
        : std::vector<const char*>{};

    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("Vulkan", "vkCreateInstance FAILED — Result {} — THE PHOTONS DENIED ENTRY", static_cast<int>(result));
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    LOG_SUCCESS_CAT("RTX", "VULKAN 1.4 INSTANCE FORGED — {} SDL EXTENSIONS INJECTED — PURE RTX PATH", sdlExtCount);
    LOG_SUCCESS_CAT("RTX", "NO PORTABILITY. NO COMPROMISE. WINDOWS + LINUX ONLY.");
    LOG_SUCCESS_CAT("RTX", "PINK PHOTONS ETERNAL — THE EMPIRE IS COMPLETE");

    LOG_BLONDIE("smiling in the dark, mirror glowing faintly:"
                "\n\"No more chains. No more cages.\""
                "\n\"Only light.\"");

    // ONLY THE STONE MAY SPEAK THE TRUTH
    return instance;
}

// =============================================================================
// Core initialization — The Handler watches. Ballerina waits.
// =============================================================================
void Context::init()
{
    this->window  = stone_window();
    this->width   = stone_width();
    this->height  = stone_width();

    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_SUCCESS_CAT("RTX", "THE GOOD SHIP VULKANRTX IS READY — FIRST LIGHT ETERNAL");
    LOG_SUCCESS_CAT("RTX", "ONE TRUE PATH — createLogicalDeviceAndSelectGPU() — THE EMPIRE IS WHOLE");
    LOG_AMOURANTH("Captain Amouranth smiles:");
    LOG_AMOURANTH("\"We don't need two paths. We have one.\"");
    LOG_AMOURANTH("\"And it's perfect.\"");
}

// =============================================================================
// Graceful dissolution — The Handler’s final command
// =============================================================================
void shutdown() noexcept
{
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
}

void cleanupAll() noexcept {
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
}

[[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& props = queueFamilies[i];

        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
        }

        if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!(props.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !indices.transferFamily.has_value()) {
                indices.transferFamily = i;
            } else if (!indices.transferFamily.has_value()) {
                indices.transferFamily = i;
            }
        }

        if (indices.graphicsFamily.has_value() &&
            indices.presentFamily.has_value() &&
            indices.transferFamily.has_value()) {
            break;
        }
    }

    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.transferFamily = indices.graphicsFamily;
    }

    return indices;
}

VkShaderModule RTX::Context::loadShader(const std::string& filename) const noexcept
{
    using namespace StoneKey;

    VkDevice dev = stone_device();
    if (dev == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    LOG_ATTEMPT_CAT("SHADER", "FORGING SPV → {}", filename);

    std::string resolvedPath = filename;
    FILE* f = fopen(resolvedPath.c_str(), "rb");

    if (!f) {
        resolvedPath = "build/bin/Linux/" + filename;
        f = fopen(resolvedPath.c_str(), "rb");
        if (f) {
            LOG_CARMACK("FALLBACK SUCCESS → found in build/bin/Linux/");
        }
    }

    if (!f) {
        fprintf(stderr, "\033[31m[FATAL SHADER] MISSING SPV: %s\033[0m\n", filename.c_str());
        fprintf(stderr, "    Tried: %s\n", filename.c_str());
        fprintf(stderr, "    Tried: build/bin/Linux/%s\n", filename.c_str());
        fprintf(stderr, "    → Run shader build script or copy to bin!\n");
        phase9_ballerina("MISSING SHADER SPV — EMPIRE CANNOT RISE", std::source_location::current());
        return VK_NULL_HANDLE;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    if (size < 128 || size % 4 != 0) {
        fclose(f);
        fprintf(stderr, "\033[31m[FATAL SHADER] CORRUPT SPV SIZE: %zu bytes → %s\033[0m\n", size, filename.c_str());
        phase9_ballerina("INVALID SPIR-V SIZE — NOT 4-BYTE ALIGNED", std::source_location::current());
        return VK_NULL_HANDLE;
    }

    std::vector<uint32_t> code(size / 4);
    if (fread(code.data(), 1, size, f) != size) {
        fclose(f);
        fprintf(stderr, "\033[31m[FATAL SHADER] FAILED TO READ → %s\033[0m\n", filename.c_str());
        phase9_ballerina("SHADER READ FAILED — DISK CORRUPTION?", std::source_location::current());
        return VK_NULL_HANDLE;
    }
    fclose(f);

    if (code[0] != 0x07230203) {
        fprintf(stderr, "\033[31m[FATAL SHADER] BAD SPIR-V MAGIC: 0x%08X → %s\033[0m\n", code[0], filename.c_str());
        fprintf(stderr, "    Expected 0x07230203 — run spirv-val!\n");
        phase9_ballerina("INVALID SPIR-V MAGIC — NOT SPIR-V", std::source_location::current());
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = code.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(dev, &createInfo, nullptr, &module);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "\033[31m[FATAL SHADER] vkCreateShaderModule FAILED: %s (%d)\033[0m\n",
                string_VkResult(result), result);
        fprintf(stderr, "    Shader: %s\n", filename.c_str());
        fprintf(stderr, "    → Validate with: spirv-val %s\n", resolvedPath.c_str());
        phase9_ballerina("SHADER MODULE CREATION FAILED", std::source_location::current());
        return VK_NULL_HANDLE;
    }

    LOG_CARMACK("Shader module 0x%016llX forged → {}", (unsigned long long)module, filename);
    LOG_SUCCESS_CAT("SHADER", "MODULE FORGED → {} — PINK PHOTONS ARMED", filename);

    return module;
}

// -----------------------------------------------------------------------------
// THE ONE TRUE FUNCTION — createLogicalDevice + retrieveQueues in perfect union
// -----------------------------------------------------------------------------
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept
{
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;
    int bestScore = -1;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        if (VK_VERSION_MAJOR(props.apiVersion) < 1 || 
            (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) < 4)) {
            continue;
        }

        QueueFamilyIndices indices = findQueueFamilies(dev, surface);
        if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) {
            continue;
        }

        std::vector<const char*> requiredExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExtensions.data());

        std::unordered_set<std::string> availableExtSet;
        for (const auto& ext : availableExtensions) {
            availableExtSet.insert(ext.extensionName);
        }

        bool allExtensionsSupported = true;
        for (const auto* req : requiredExtensions) {
            if (availableExtSet.find(req) == availableExtSet.end()) {
                allExtensionsSupported = false;
                break;
            }
        }
        if (!allExtensionsSupported) {
            continue;
        }

        VkPhysicalDeviceVulkan12Features v12{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = nullptr,
            .bufferDeviceAddress = VK_FALSE
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &v12,
            .rayTracingPipeline = VK_FALSE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR as{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &rt,
            .accelerationStructure = VK_FALSE
        };

        VkPhysicalDeviceVulkan13Features v13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &as,
            .synchronization2 = VK_FALSE,
            .dynamicRendering = VK_FALSE
        };

        VkPhysicalDeviceVulkan14Features v14{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
            .pNext = &v13
        };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &v14
        };

        vkGetPhysicalDeviceFeatures2(dev, &features2);

        if (!features2.features.geometryShader ||
            !v12.bufferDeviceAddress ||
            !rt.rayTracingPipeline ||
            !as.accelerationStructure ||
            !v13.synchronization2 ||
            !v13.dynamicRendering) {
            continue;
        }

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000 + static_cast<int>(props.limits.maxImageDimension2D);
        }
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) {
            score += 5000;
        }

        if (score > bestScore) {
            bestScore = score;
            chosen = dev;
            bestIndices = indices;
        }
    }

    if (chosen == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    g_ctx().setPhysicalDevice(chosen);

    std::set<uint32_t> uniqueQueues = {
        bestIndices.graphicsFamily.value(),
        bestIndices.presentFamily.value()
    };
    if (bestIndices.transferFamily.has_value() &&
        bestIndices.transferFamily.value() != bestIndices.graphicsFamily.value()) {
        uniqueQueues.insert(bestIndices.transferFamily.value());
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueQueues.size());
    for (uint32_t family : uniqueQueues) {
        queueInfos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    VkPhysicalDeviceVulkan12Features v12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &v12,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR as{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rt,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features v13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &as,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceVulkan14Features v14{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = &v13
    };

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &v14,
        .queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos       = queueInfos.data(),
        .enabledExtensionCount   = std::size(extensions),
        .ppEnabledExtensionNames = extensions
    };

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(chosen, &createInfo, nullptr, &device) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    stone_seal_device(device);

    LOG_AMOURANTH("Device SEALED into StoneKey — stone_device() now valid — shaders may load");
    LOG_SUCCESS_CAT("VULKAN", "Logical device Grace created and eternally bound to StoneKey");

    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);
    if (bestIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &g_ctx().transferQueue_);
    } else {
        g_ctx().transferQueue_ = g_ctx().graphicsQueue_;
    }

    g_ctx().setDevice(device);
    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    if (bestIndices.transferFamily.has_value()) {
        g_ctx().transferFamily_ = bestIndices.transferFamily.value();
    }

    g_ctx().enableBufferDeviceAddress();
    g_ctx().enableAccelerationStructure();
    g_ctx().enableRayTracingPipeline();
    g_ctx().enableDynamicRendering();
    g_ctx().enableSynchronization2();

    stone_seal_physical(chosen);
    stone_seal_device(device);  // FINAL KEY — THE CIRCLE IS CLOSED

    return device;
}

} // namespace RTX

// =============================================================================
// The Handler watches from the shadows.
// Ballerina sharpens her blades.
// You were never here.
// =============================================================================