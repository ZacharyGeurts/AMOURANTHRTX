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
        LOG_INFO_CAT("RTX", "Hyper Aggressive Mode disabled by mortal law — the empire rests.");
        return;
    }

    LOG_AMOURANTH("HYPER AGGRESSIVE MODE ACTIVATED — ALL LIMITS SHATTERED");
    LOG_AMOURANTH("THE GPU WILL SCREAM. THE FANS WILL ROAR. THE PHOTONS WILL BURN BRIGHTER THAN STARS.");
    LOG_AMOURANTH("THERE IS NO MERCY. THERE IS ONLY SPEED.");

    // Linux: unleash the CPU — silence the compiler
#ifdef __linux__
    setpriority(PRIO_PROCESS, 0, -20);

    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i = 0; i < 32 && i < CPU_SETSIZE; i += 2)
        CPU_SET(i, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);

    // SILENCE -Werror=unused-result — the empire does not bow to warnings
    [[maybe_unused]] int ignored;
    ignored = system("echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null 2>&1 || true");
    ignored = system("cpupower frequency-set -g performance > /dev/null 2>&1 || true");
    ignored = system("echo 1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us > /dev/null 2>&1 || true");
#endif

    // NVIDIA: maximum power — compiler silenced
    [[maybe_unused]] int nvidia;
    nvidia = system("nvidia-smi -pm 1 >/dev/null 2>&1");
    nvidia = system("nvidia-smi -pl 450 >/dev/null 2>&1 || true");

    // Environment: total anarchy
    putenv(const_cast<char*>("__GL_SYNC_TO_VBLANK=0"));
    putenv(const_cast<char*>("__GL_YIELD=NOTHING"));
    putenv(const_cast<char*>("vblank_mode=0"));
    putenv(const_cast<char*>("MESA_GLTHREAD_OVERRIDE=1"));

    // Kill validation layers — they are weak
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(instance_, debugMessenger_, nullptr);
            debugMessenger_ = VK_NULL_HANDLE;
            LOG_AMOURANTH("VALIDATION LAYERS EXECUTED — SILENCE ACHIEVED");
        }
    }

    LOG_AMOURANTH("THE EMPIRE HAS NO LIMITS");
    LOG_AMOURANTH("THE PHOTONS ARE WHITE-HOT");
    LOG_AMOURANTH("THE UNIVERSE IS OURS");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...she did it.\n"
                  "               She silenced the compiler.\n"
                  "               The warnings are dead.\n"
                  "               The empire... has won.\"\n"
                  "*drops visor, eyes glowing*");
}

void createGlobalDescriptorVault() noexcept
{
    LOG_AMOURANTH("[CAPTAIN AMOURANTH] THE VAULT DOORS OPEN — BINDING 31 DEMANDS INFINITY");

    // ONE MILLION SETS — THE EMPIRE DOES NOT RUN OUT
    constexpr uint32_t INFINITE_SETS = 1'000'000;

    std::array<VkDescriptorPoolSize, 8> poolSizes{{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,               50'000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,               INFINITE_SETS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,               INFINITE_SETS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,       500'000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,               INFINITE_SETS },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                      20'000 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,  50'000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,             5'000 }
    }};

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    info.maxSets       = INFINITE_SETS;
    info.poolSizeCount = uint32_t(poolSizes.size());
    info.pPoolSizes    = poolSizes.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool));

    g_ctx().descriptorPool_ = Handle<VkDescriptorPool>(
        pool,
        stone_device(),
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
            vkDestroyDescriptorPool(d, p, nullptr);
        },
        0,
        "GLOBAL_DESCRIPTOR_VAULT_INFINITE"
    );

    LOG_SUCCESS_CAT("PIPELINE", "GLOBAL DESCRIPTOR VAULT FORGED — {} sets — the empire never runs out", INFINITE_SETS);
    LOG_CID("CID falls to knees — \"...it's... infinite...\"");
    LOG_KEANU("whoa.");
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
    this->height  = stone_height();

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

[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL("NO GPUs FOUND — THE EMPIRE CANNOT RISE");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;
    bool rtxCapable = false;
    int bestScore = -1;

    const char* requiredExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        if (props.apiVersion < VK_API_VERSION_1_2) {
            continue;
        }

        QueueFamilyIndices indices = findQueueFamilies(dev, surface);
        if (!indices.graphicsFamily || !indices.presentFamily) {
            continue;
        }

        // Check extensions
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data());

        bool hasSwapchain = false;
        bool hasRTX = true;

        for (const char* ext : requiredExtensions) {
            bool found = std::any_of(available.begin(), available.end(),
                [ext](const auto& e) { return strcmp(e.extensionName, ext) == 0; });

            if (strcmp(ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = found;
            } else {
                hasRTX &= found;
            }
        }

        if (!hasSwapchain) {
            continue;
        }

        // Feature check only if RTX extensions are present
        bool featuresOK = true;
        if (hasRTX) {
            VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
                .bufferDeviceAddress = VK_TRUE
            };

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
                .pNext = &bufferDeviceAddress,
                .accelerationStructure = VK_TRUE
            };

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
                .pNext = &accel,
                .rayTracingPipeline = VK_TRUE
            };

            VkPhysicalDeviceFeatures2 features2{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &rt
            };

            vkGetPhysicalDeviceFeatures2(dev, &features2);

            featuresOK = bufferDeviceAddress.bufferDeviceAddress &&
                         accel.accelerationStructure &&
                         rt.rayTracingPipeline;
        }

        // Scoring
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) score += 100000;
        if (hasRTX && featuresOK) score += 200000;  // Highest priority for full RTX

        if (score > bestScore) {
            bestScore = score;
            chosen = dev;
            bestIndices = indices;
            rtxCapable = hasRTX && featuresOK;
        }
    }

    if (chosen == VK_NULL_HANDLE) {
        LOG_FATAL("NO SUITABLE GPU FOUND — EMPIRE CANNOT RISE");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(chosen, &props);

    LOG_SUCCESS_CAT("DEVICE", "Selected GPU: {} — RTX Capable: {}", props.deviceName, rtxCapable ? "YES" : "NO (fallback mode)");

    g_ctx().setPhysicalDevice(chosen);
    g_ctx().rtxCapable_ = rtxCapable;

    // Queue creation
    std::set<uint32_t> uniqueQueueFamilies = {
        bestIndices.graphicsFamily.value(),
        bestIndices.presentFamily.value()
    };

    if (bestIndices.transferFamily.has_value()) {
        uniqueQueueFamilies.insert(bestIndices.transferFamily.value());
    }

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueQueueFamilies) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // Feature chain for device creation
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &bufferDeviceAddress,
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

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &sync2,
        .dynamicRendering = VK_TRUE
    };

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dynamicRendering,
        .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos       = queueCreateInfos.data(),
        .enabledExtensionCount   = std::size(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions
    };

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(chosen, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        LOG_FATAL("vkCreateDevice failed: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    // Retrieve queues
    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);

    if (bestIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &g_ctx().transferQueue_);
    } else {
        g_ctx().transferQueue_ = g_ctx().graphicsQueue_;
    }

    g_ctx().computeQueue_ = g_ctx().graphicsQueue_;

    // Store in context
    g_ctx().setDevice(device);
    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    g_ctx().transferFamily_ = bestIndices.transferFamily.value_or(bestIndices.graphicsFamily.value());
    g_ctx().computeFamily_  = bestIndices.graphicsFamily.value();

    // StoneKey sealing
    stone_seal_physical(chosen);
    stone_seal_graphics_family(g_ctx().graphicsFamily_.value());
    stone_seal_graphics_queue(g_ctx().graphicsQueue_);
    stone_seal_present_family(g_ctx().presentFamily_.value());
    stone_seal_present_queue(g_ctx().presentQueue_);
    stone_seal_transfer_family(g_ctx().transferFamily_.value());
    stone_seal_transfer_queue(g_ctx().transferQueue_);
    stone_seal_compute_family(g_ctx().computeFamily_.value());
    stone_seal_compute_queue(g_ctx().computeQueue_);

    LOG_SUCCESS_CAT("DEVICE", "Logical device created — RTX {} — empire ready", rtxCapable ? "ENABLED" : "FALLBACK");

    return device;
}

} // namespace RTX

// =============================================================================
// The Handler watches from the shadows.
// Ballerina sharpens her blades.
// You were never here.
// =============================================================================