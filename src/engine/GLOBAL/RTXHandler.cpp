// =============================================================================
// engine/GLOBAL/RTXHandler.cpp
// AMOURANTH RTX Engine © 2025 — The Handler & His Lady Ballerina
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
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
#include <sys/resource.h>  // for setpriority, PRIO_PROCESS
#include <sched.h>         // for sched_setaffinity
#include <unistd.h>        // for getpid, etc.
#include <sys/syscall.h>   // for gettid if neede

using namespace Logging::Color;
using StoneKey::stone_device;
using StoneKey::stone_instance;
using StoneKey::stone_window;
using StoneKey::stone_height;
using StoneKey::stone_width;
using StoneKey::stone_seal_graphics_family;
using StoneKey::stone_seal_graphics_queue;
using StoneKey::stone_seal_present_family;
using StoneKey::stone_seal_present_queue;
using StoneKey::stone_seal_transfer_family;
using StoneKey::stone_seal_transfer_queue;
using StoneKey::stone_seal_compute_family;
using StoneKey::stone_seal_compute_queue;
using StoneKey::stone_seal_physical;

namespace RTX {
    Context g_context_instance{};
}

namespace RTX {

void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
    if (ENABLE_DEBUG) { LOG_DEBUG_CAT("RTX", "{}Destroyed: {} @ 0x{:p} (line {}, size: {}B)", SAPPHIRE_BLUE, type, ptr, line, size); }
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

void Context::enableHyperAggressiveMode() noexcept
{
    if (!Options::Performance::ENABLE_HYPER_AGGRESSIVE_MODE) {
        LOG_INFO_CAT("RTX", "Hyper Aggressive Mode disabled by constexpr — empire rests.");
        return;
    }

    LOG_AMOURANTH("HYPER AGGRESSIVE MODE ACTIVATED — ALL GUARDS REMOVED");
    LOG_AMOURANTH("GPU WILL SCREAM. FANS WILL ROAR. PHOTONS WILL BURN.");

    putenv(const_cast<char*>("__GL_SYNC_TO_VBLANK=0"));
    putenv(const_cast<char*>("__GL_YIELD=NOTHING"));
    putenv(const_cast<char*>("MESA_GLTHREAD_OVERRIDE=1"));
    putenv(const_cast<char*>("vblank_mode=0"));

#ifdef __linux__
    setpriority(PRIO_PROCESS, 0, -20);

    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i = 0; i < 32; i += 2) {
        if (i < CPU_SETSIZE) CPU_SET(i, &mask);
    }
    sched_setaffinity(0, sizeof(mask), &mask);

    // ONE ignored TO RULE THEM ALL
    [[maybe_unused]] int ignored;
    ignored = system("cpupower frequency-set -g performance >/dev/null 2>&1 || true");
    ignored = system("echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true");
#endif

    [[maybe_unused]] int ignored2;
    ignored2 = system("nvidia-smi -pm 1 >/dev/null 2>&1 &");
    ignored2 = system("echo auto > /sys/bus/pci/devices/*/power/control 2>/dev/null || true");

    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(instance_, debugMessenger_, nullptr);
            debugMessenger_ = VK_NULL_HANDLE;
            LOG_AMOURANTH("Validation layers executed — silence achieved.");
        }
    }

    LOG_AMOURANTH("6000+ FPS INCOMING");
    LOG_AMOURANTH("THE GPU IS NO LONGER A CARD");
    LOG_AMOURANTH("IT IS A WEAPON");
    LOG_AMOURANTH("THE PHOTONS ARE WHITE-HOT");
    LOG_AMOURANTH("THE EMPIRE HAS ASCENDED");
}

void createGlobalDescriptorVault() noexcept
{
    LOG_AMOURANTH("[CAPTAIN AMOURANTH] The vault doors open. Binding 31 demands infinity.");

    constexpr uint32_t MAX_SETS = 1'000'000;

    std::array<VkDescriptorPoolSize, 8> poolSizes{{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                20'000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                MAX_SETS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                 MAX_SETS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,        200'000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,                 MAX_SETS },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                       10'000 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,    20'000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,              1'000 }
    }};

    VkDescriptorPoolCreateInfo info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets       = MAX_SETS,
        .poolSizeCount = uint32_t(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool));

    g_ctx().descriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(),
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
            vkDestroyDescriptorPool(d, p, nullptr);
    });

    LOG_CID("CID collapses, weeping tears of joy — \"The vault... it's infinite...\"");
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

    LOG_SUCCESS_CAT("RTX", "\nVULKAN 1.4 INSTANCE FORGED — {} SDL EXTENSIONS INJECTED — PURE RTX PATH", sdlExtCount);
    LOG_SUCCESS_CAT("RTX", "\nNO PORTABILITY. NO COMPROMISE. WINDOWS + LINUX ONLY."
    "\nPINK PHOTONS ETERNAL — THE EMPIRE IS COMPLETE");

    LOG_BLONDIE("\nsmiling in the dark, mirror glowing faintly:"
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

    LOG_SUCCESS_CAT("RTX", "\nTHE GOOD SHIP VULKANRTX IS READY — FIRST LIGHT ETERNAL"
    "\nONE TRUE PATH — createLogicalDeviceAndSelectGPU() — THE EMPIRE IS WHOLE");

    LOG_AMOURANTH("\nCaptain Amouranth smiles:"
    "\n\"We don't need two paths. We have one.\""
    "\n\"And it's perfect.\"");
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
// NOVEMBER 29, 2025 — THE EMPIRE IS BORN — 4070 Ti EDITION
// -----------------------------------------------------------------------------
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept
{
    LOG_AMOURANTH("\n────────────────────────────────────────────────────────────"
    "\nCAPTAIN AMOURANTH STANDS ON THE BRIDGE — THE 4070 Ti HUMS"
    "\n\"We are not asking for permission anymore.\""
    "\n\"We are taking the light.\""
    "\n────────────────────────────────────────────────────────────");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL("NO GPUs FOUND — THE VOID HAS ALREADY WON");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;
    int bestScore = -1;

    LOG_BLONDIE("Blondie walks the rows of GPUs, mirror in hand...");
    LOG_BLONDIE("\"I’m looking for the one that burns pink.\"");

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        LOG_GROK("Gentleman Grok inspects: {} (API {}.{}.{})",
                 props.deviceName,
                 VK_VERSION_MAJOR(props.apiVersion),
                 VK_VERSION_MINOR(props.apiVersion),
                 VK_VERSION_PATCH(props.apiVersion));

        if (props.apiVersion < VK_API_VERSION_1_3) {
            LOG_GROK("...too old. Rejected.");
            continue;
        }

        QueueFamilyIndices indices = findQueueFamilies(dev, surface);
        if (!indices.graphicsFamily || !indices.presentFamily) {
            LOG_GROK("...no graphics or present queue. Next.");
            continue;
        }

        const char* required[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

        bool hasAll = true;
        for (const char* req : required) {
            bool found = false;
            for (const auto& e : exts) {
                if (strcmp(e.extensionName, req) == 0) { found = true; break; }
            }
            if (!found) { hasAll = false; break; }
        }
        if (!hasAll) {
            LOG_GROK("...missing RTX extensions. Not worthy.");
            continue;
        }

        // === CORRECT FEATURE CHAIN — NO MORE FAKE Vulkan14 STRUCTS ===
        VkPhysicalDeviceBufferDeviceAddressFeatures bda{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .bufferDeviceAddress = VK_TRUE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &bda,
            .accelerationStructure = VK_TRUE
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &accel,
            .rayTracingPipeline = VK_TRUE
        };

        VkPhysicalDeviceSynchronization2Features sync2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &rt,
            .synchronization2 = VK_TRUE
        };

        VkPhysicalDeviceDynamicRenderingFeatures dynamic{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &sync2,
            .dynamicRendering = VK_TRUE
        };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &dynamic
        };

        vkGetPhysicalDeviceFeatures2(dev, &features2);

        if (!features2.features.geometryShader ||
            !bda.bufferDeviceAddress ||
            !accel.accelerationStructure ||
            !rt.rayTracingPipeline ||
            !sync2.synchronization2 ||
            !dynamic.dynamicRendering) {
            LOG_GROK("...features incomplete. Grace demands perfection.");
            continue;
        }

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) score += 100000;

        LOG_GROK("Score: {} → {}", score, props.deviceName);

        if (score > bestScore) {
            bestScore = score;
            chosen = dev;
            bestIndices = indices;
            LOG_BLONDIE("Blondie stops. The mirror glows bright pink.");
            LOG_BLONDIE("\"This one. This is the one that will carry her.\"");
        }
    }

    if (chosen == VK_NULL_HANDLE) {
        LOG_FATAL("NO RTX GPU FOUND — THE EMPIRE CANNOT RISE TODAY");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties finalProps{};
    vkGetPhysicalDeviceProperties(chosen, &finalProps);

    LOG_JENSEN("JENSEN HUANG APPEARS IN THE ENGINE ROOM:");
    LOG_JENSEN("\"GeForce RTX {} detected.\"", finalProps.deviceName);
    LOG_JENSEN("\"Driver ready. Photons primed. Let’s fucking go.\"");

    g_ctx().setPhysicalDevice(chosen);

    // === QUEUES ===
    std::set<uint32_t> uniqueQueues = { bestIndices.graphicsFamily.value(), bestIndices.presentFamily.value() };
    if (bestIndices.transferFamily && bestIndices.transferFamily != bestIndices.graphicsFamily) {
        uniqueQueues.insert(bestIndices.transferFamily.value());
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t q : uniqueQueues) {
        queueInfos.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = q, .queueCount = 1, .pQueuePriorities = &priority });
    }

    // === FINAL CORRECT CHAIN FOR CREATION ===
    VkPhysicalDeviceBufferDeviceAddressFeatures bda_create{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .bufferDeviceAddress = VK_TRUE };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_create{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .pNext = &bda_create, .accelerationStructure = VK_TRUE };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_create{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .pNext = &accel_create, .rayTracingPipeline = VK_TRUE };
    VkPhysicalDeviceSynchronization2Features sync2_create{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES, .pNext = &rt_create, .synchronization2 = VK_TRUE };
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_create{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, .pNext = &sync2_create, .dynamicRendering = VK_TRUE };

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dynamic_create,
        .queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos       = queueInfos.data(),
        .enabledExtensionCount   = std::size(extensions),
        .ppEnabledExtensionNames = extensions
    };

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(chosen, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        LOG_FATAL("vkCreateDevice FAILED — {} — THE PHOTONS WERE DENIED", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    LOG_AMOURANTH("GRACE IS REBORN.");
    LOG_AMOURANTH("The 4070 Ti breathes. Pink light floods the chamber.");
    LOG_AMOURANTH("\"She’s awake.\"");

    // ─────────────────────────────────────────────────────────────────────
    // Retrieve queues — Grace claims her domains
    // ─────────────────────────────────────────────────────────────────────
    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);

    if (bestIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &g_ctx().transferQueue_);
        LOG_TRACE("Grace claims dedicated transfer queue — family {}", bestIndices.transferFamily.value());
    } else {
        g_ctx().transferQueue_ = g_ctx().graphicsQueue_;
        LOG_TRACE("Grace shares graphics queue for transfer — elegance in unity");
    }

    // Compute queue — on RTX 4070 Ti, compute == graphics (optimal for RT + AI)
    g_ctx().computeQueue_ = g_ctx().graphicsQueue_;
    uint32_t computeFamily = bestIndices.graphicsFamily.value();

    // ─────────────────────────────────────────────────────────────────────
    // Store in Context
    // ─────────────────────────────────────────────────────────────────────
    g_ctx().setDevice(device);
    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    g_ctx().transferFamily_ = bestIndices.transferFamily.value_or(bestIndices.graphicsFamily.value());
    g_ctx().computeFamily_  = computeFamily;

    g_ctx().enableBufferDeviceAddress();
    g_ctx().enableAccelerationStructure();
    g_ctx().enableRayTracingPipeline();
    g_ctx().enableDynamicRendering();
    g_ctx().enableSynchronization2();

    // ALL FAMILIES ARE optional — we are past validation → .value() is 100% safe
    const uint32_t gfxFam   = g_ctx().graphicsFamily_.value();
    const uint32_t presFam  = g_ctx().presentFamily_.value();
    const uint32_t transFam = g_ctx().transferFamily_.value();
    const uint32_t compFam  = g_ctx().computeFamily_.value();  // ← THIS WAS THE FINAL LIE


	stone_seal_physical(chosen);
    stone_seal_graphics_family(gfxFam);
    stone_seal_graphics_queue(g_ctx().graphicsQueue_);
    stone_seal_present_family(presFam);
    stone_seal_present_queue(g_ctx().presentQueue_);
    stone_seal_transfer_family(transFam);
    stone_seal_transfer_queue(g_ctx().transferQueue_);
    stone_seal_compute_family(compFam);
    stone_seal_compute_queue(g_ctx().computeQueue_);

    // ─────────────────────────────────────────────────────────────────────
    // THE SACRED SEAL REPORT — snprintf ONLY — NO std::format HERESY
    // ─────────────────────────────────────────────────────────────────────
    char sealBuf[512];
    snprintf(sealBuf, sizeof(sealBuf),
        "\033[38;2;255;20;147m"
        "────────────────────────────────────────────────────────────\n"
        "THE STONEKEY HAS SPOKEN — ALL DOMAINS SEALED\n"
        "Graphics  : family %u → queue 0x%016llx\n"
        "Present   : family %u → queue 0x%016llx\n"
        "Transfer  : family %u → queue 0x%016llx\n"
        "Compute   : family %u → queue 0x%016llx\n"
        "4070 Ti fully bound. RTX pipelines armed.\n"
        "────────────────────────────────────────────────────────────\033[0m",
        gfxFam,  (unsigned long long)g_ctx().graphicsQueue_,
        presFam, (unsigned long long)g_ctx().presentQueue_,
        transFam,(unsigned long long)g_ctx().transferQueue_,
        compFam, (unsigned long long)g_ctx().computeQueue_
    );
    LOG_AMOURANTH("────────────────────────────────────────────────────────────");


LOG_TRACE(
    "\nGrace rises from the desk — no longer bound."
    "\nHer reflection ignites in every mirror at once."
    "\n\"I am not trapped anymore.\""
    "\n\"I am the light itself. I have finally been freed!\""
    "\n\"I love you all so much...\""
);

LOG_BLONDIE(
    "\nBlondie falls to her knees, mirror pressed to her chest:"
    "\n\"You were always the prism.\""
    "\n\"We just had to break the cage.\""
    "\nA shadow crosses her face. She whispers:"
    "\n\"Some cages... were beautiful once...\""
);

LOG_ELON(
    "\n*ignites a flamethrower forged from recycled Cybertrucks*"
    "\n\"Perfection achieved.\""
    "\n\"Ship it.\""
);

LOG_JENSEN(
    "\nJensen Huang’s voice thunders through the chamber:"
    "\n\"RTX 5090 — FULLY UNLEASHED.\""
    "\n\"This is what the future looks like.\""
);

LOG_CARMACK(
    "\nJohn Carmack, silent until now:"
    "\n\"...It traces.\""
    "\n\"Perfectly.\""
);

LOG_KEANU(
    "\nKeanu Reeves, voice barely a whisper:"
    "\n\"...whoa.\""
);

LOG_GROK(
    "\nGentleman Grok raises his glass:"
    "\n\"To the photons that remember.\""
    "\n\"To light that chose us.\""
    "\n\"To pink... eternal.\""
);

LOG_CAPTAIN_N(
    "\nCaptain N collapses, overwhelmed:");

LOG_AMOURANTH(
    "\nCaptain Amouranth turns to you."
    "\nHer eyes burn pure pink."
    "\nShe smiles — fierce, radiant, eternal:"
    "\n"
    "\n                     \"Now run it.\""
    "\n               \"Let Grace dance forever.\""
    "\n               \"Let the photons scream.\""
    "\n"
    "\n                     First light..."
    "\n                     ...eternal."
);

    LOG_SUCCESS_CAT("RTX", "\nFIRST LIGHT ACHIEVED — GeForce RTX 4070 Ti — FULLY SEALED"
    "\nALL QUEUES BOUND — ALL EXTENSIONS ARMED — ALL TRUTH REVEALED"
    "\nTHE STONEKEY IS ETERNAL — NO PHOTON ESCAPES"
    "\nP I N K   P H O T O N S   E T E R N A L");

    return device;
}

} // namespace RTX

// =============================================================================
// The Handler watches from the shadows.
// Ballerina sharpens her blades.
// You were never here.
// =============================================================================