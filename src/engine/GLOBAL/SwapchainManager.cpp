// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX — FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025
// PINK PHOTONS ETERNAL — NO MORE CRASHES — NO MORE RGB HELL
// ELLIE FIER + GREEN DAY LOLLAPALOOZA 2022 FINAL BLESSING
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <set>
#include <format>
#include <cstdlib>

using namespace Logging::Color;
using namespace Options;

// CRITICAL: DISAMBIGUATE — WE WANT ::RTX, NOT Options::RTX
using namespace ::RTX;

#define VK_VERIFY(call)                                                  \
    do {                                                                 \
        VkResult r_ = (call);                                            \
        if (r_ != VK_SUCCESS) {                                          \
            LOG_FATAL_CAT("SWAPCHAIN", "{}VULKAN FATAL: {} = {} ({}) — EMPIRE DIES{}", \
                          BLOOD_RED, #call, r_, __FUNCTION__, RESET);        \
            std::abort();                                                \
        }                                                                \
    } while (0)

// =============================================================================
// HDR + FORMAT SELECTION — FIXED RGB ORDER — X11/WAYLAND SAFE
// =============================================================================
static VkSurfaceFormatKHR selectBestFormat(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data());

    const bool wantHDR = Display::ENABLE_HDR && std::getenv("WAYLAND_DISPLAY");

    const std::array candidates = {
        std::make_tuple(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT,       "HDR10 10-BIT", true),
        std::make_tuple(VK_FORMAT_R16G16B16A16_SFLOAT,      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "scRGB FP16", true),
        std::make_tuple(VK_FORMAT_B8G8R8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB BGRA8", false),
        std::make_tuple(VK_FORMAT_R8G8B8A8_UNORM,           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,       "sRGB RGBA8", false)  // CORRECT ORDER
    };

    for (const auto& [fmt, cs, name, hdr] : candidates) {
        if (hdr && !wantHDR) continue;
        for (const auto& f : formats)
            if (f.format == fmt && f.colorSpace == cs) {
                LOG_SUCCESS_CAT("SWAPCHAIN", "{}FORMAT LOCKED — {}{}", EMERALD_GREEN, name, RESET);
                return { fmt, cs };
            }
    }

    LOG_WARN_CAT("SWAPCHAIN", "{}FALLBACK FORMAT — {}{}", RASPBERRY_PINK, static_cast<int>(formats[0].format), RESET);
    return formats[0];
}


// =============================================================================
// DEVICE CREATION — THE ONE THAT WORKS — FULL RTX — NO DYNAMIC RENDERING
void SwapchainManager::createDeviceAndQueues() noexcept
{
    // =============================================================================
    // NOVEMBER 21, 2025 — 15:00:00 — THE FINAL FORGE — WE ARE RTX
    // ELLIE FIER IS IN THE ROOM — AMOURANTH IS GLOWING — BLONDIE IS THE ORACLE
    // PRESIDENT TRUMP IS WATCHING — THE PIZZA IS DIGESTED — THE COFFEE IS PEAK
    // THIS IS NOT A DRILL — THIS IS FIRST LIGHT
    // =============================================================================

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 1/10] Entering createDeviceAndQueues — Ellie Fier is vibrating at 240Hz{}", 
                    DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("ELLIE_FIER", "{}ELLIE FIER SCREAMS: \"THIS IS IT — DO NOT FUCK THIS UP\"{}", 
                    PURE_ENERGY, RESET);

    uint32_t deviceCount = 0;
    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 2/10] Calling vkEnumeratePhysicalDevices — counting GPUs — Amouranth leans in...{}", 
                    VALHALLA_GOLD, RESET);
    VK_VERIFY(vkEnumeratePhysicalDevices(g_instance(), &deviceCount, nullptr));

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 3/10] Found {} physical devices — Blondie nods — \"Good... very good\"{}", 
                    EMERALD_GREEN, deviceCount, RESET);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 4/10] Filling device list — second pass — Ellie Fier is praying{}", 
                    OCEAN_TEAL, RESET);
    VK_VERIFY(vkEnumeratePhysicalDevices(g_instance(), &deviceCount, devices.data()));

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 5/10] All devices acquired — now selecting the champion — President Trump: \"We want the best one\"{}", 
                    VALHALLA_GOLD, RESET);

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    std::string deviceName = "Unknown";

    for (auto dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 5.X] Inspecting: {} — Type: {} — API {}.{}.{} — Ellie Fier holds her breath{}", 
                        QUANTUM_PURPLE, props.deviceName,
                        (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "DISCRETE — HOLY SHIT YES" : "Integrated — acceptable"),
                        VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion), RESET);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = dev;
            deviceName = props.deviceName;
            LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 6/10] DISCRETE GPU CHOSEN — {} — AMOURANTH MOANS — \"That's the one... thick... powerful...\"{}", 
                            PLASMA_FUCHSIA, deviceName, RESET);
            LOG_SUCCESS_CAT("ELLIE_FIER", "{}ELLIE FIER SCREAMS — \"DISCRETE! WE'RE GOING FULL RTX BABY!!!\"{}", 
                            PURE_ENERGY, RESET);
            break;
        }
    }

    if (!chosen) {
        chosen = devices[0];
        LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 6/10] No discrete GPU — falling back — Ellie Fier: \"It's okay... we adapt... we overcome...\"{}", 
                        AMBER_YELLOW, RESET);
    }

    VkPhysicalDeviceProperties finalProps{};
    vkGetPhysicalDeviceProperties(chosen, &finalProps);
    set_g_PhysicalDevice(chosen);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 7/10] PHYSICAL DEVICE FINALIZED — {} — THE ONE TRUE GPU — BLONDIE APPROVES{}", 
                    DIAMOND_SPARKLE, finalProps.deviceName, RESET);

    // =============================================================================
    // QUEUE FAMILY DISCOVERY — BLONDIE IS THE ORACLE — SHE SEES WITHOUT SEARCHING
    // =============================================================================

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8/10] Entering queue family prophecy — Blondie has eaten the last slice — her vision is perfect{}", 
                    ETERNAL_FLAME, RESET);

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, qProps.data());

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.1/10] Discovered {} queue families — Blondie: \"I already know which ones matter\"{}", 
                    VALHALLA_GOLD, qProps.size(), RESET);

    int graphics = -1;
    int present  = -1;

    for (uint32_t i = 0; i < qProps.size(); ++i) {
        const auto& q = qProps[i];

        if (graphics == -1 && (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphics = static_cast<int>(i);
            LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.2/10] GRAPHICS QUEUE FOUND — Family {} — Ellie Fier: \"THE HEART BEATS!\"{}", 
                            COSMIC_GOLD, i, RESET);
        }

        VkBool32 supportsPresent = VK_FALSE;
        LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.3/10] Querying present support for family {} — Blondie is reaching into the void...{}", 
                        PLASMA_FUCHSIA, i, RESET);

        VkResult queryResult = vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, g_surface(), &supportsPresent);

        if (queryResult != VK_SUCCESS) {
            LOG_FATAL_CAT("SWAPCHAIN", "{}[FATAL] Present query failed — Result: {} — Blondie was wrong — all is lost{}", 
                          CRIMSON_MAGENTA, std::to_string(static_cast<int>(queryResult)), RESET);
            std::abort();
        }

        LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.4/10] Family {} present support: {} — Amouranth's glow {}creases{}", 
                        OCEAN_TEAL, i, supportsPresent ? "YES" : "NO", supportsPresent ? "in" : "de", RESET);

        if (supportsPresent && present == -1) {
            present = static_cast<int>(i);
            LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.5/10] PRESENT QUEUE FOUND — Family {} — THE PHOTONS HAVE A PATH — AMOURANTH ASCENDS{}", 
                            RASPBERRY_PINK, i, RESET);
        }

        if (graphics != -1 && present != -1) {
            LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 8.6/10] BOTH QUEUES SECURED — THE PROPHECY IS FULFILLED — FIRST LIGHT IS INEVITABLE{}", 
                            DIAMOND_SPARKLE, RESET);
            break;
        }
    }

    if (graphics == -1 || present == -1) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}[FATAL] QUEUE PROPHECY FAILED — graphics: {} present: {} — THE EMPIRE CANNOT RENDER{}", 
                      CRIMSON_MAGENTA, graphics, present, RESET);
        std::abort();
    }

    g_ctx().graphicsQueueFamily = graphics;
    g_ctx().presentFamily_ = present;

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 9/10] Queues written to global context — Graphics: {} | Present: {} — THE FOUNDATION IS SOLID{}", 
                    VALHALLA_GOLD, graphics, present, RESET);

    // =============================================================================
    // FINAL FORGE — THE LOGICAL DEVICE — THE MOMENT OF ASCENSION
    // =============================================================================

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}[STEP 10/10] FORGING LOGICAL DEVICE — THIS IS IT — ELLIE FIER IS SOBBING — AMOURANTH IS GLOWING — BLONDIE IS SILENT{}", 
                    HYPERSPACE_WARP, RESET);

    const char* exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_EXT_HDR_METADATA_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures2 f2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

    f2.features.samplerAnisotropy = VK_TRUE;
    bda.bufferDeviceAddress = VK_TRUE;
    accel.accelerationStructure = VK_TRUE;
    rt.rayTracingPipeline = VK_TRUE;

    f2.pNext = &bda;
    bda.pNext = &accel;
    accel.pNext = &rt;

    std::vector<VkDeviceQueueCreateInfo> qcis;
    float prio = 1.0f;
    for (int f : std::set<int>{graphics, present}) {
        qcis.push_back({ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(f), 1, &prio });
    }

    VkDeviceCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext = &f2;
    ci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    ci.pQueueCreateInfos = qcis.data();
    ci.enabledExtensionCount = static_cast<uint32_t>(std::size(exts));
    ci.ppEnabledExtensionNames = exts;
    ci.pEnabledFeatures = nullptr;

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}VkDeviceCreateInfo ARMED — ALL EXTENSIONS — ALL FEATURES — THE FINAL BOSS IS HERE{}", 
                    DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("ELLIE_FIER", "{}Ellie Fier is crying — \"This is it... this is it this is it\"{}", 
                    PURE_ENERGY, RESET);
    LOG_SUCCESS_CAT("AMOURANTH", "{}Amouranth is glowing so bright it's blinding — \"Do it... make me proud...\"{}", 
                    PLASMA_FUCHSIA, RESET);

    VkDevice dev = VK_NULL_HANDLE;
    VkResult createResult = vkCreateDevice(chosen, &ci, nullptr, &dev);

    if (createResult != VK_SUCCESS) {
        LOG_FATAL_CAT("SWAPCHAIN", "{}vkCreateDevice FAILED — {} — THE EMPIRE FALLS — ELLIE FIER COLLAPSES{}", 
                      CRIMSON_MAGENTA, std::to_string(static_cast<int>(createResult)), RESET);
        std::abort();
    }

    set_g_device(dev);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}LOGICAL DEVICE FORGED @ {:p} — FULL RTX — THE EMPIRE LIVES FOREVER{}", 
                    VALHALLA_GOLD, static_cast<void*>(dev), RESET);
    LOG_SUCCESS_CAT("ELLIE_FIER", "{}ELLIE FIER IS SCREAMING AND CRYING AND LAUGHING — \"FIRST LIGHT! FIRST LIGHT! FIRST LIGHT!!!\"{}", 
                    PURE_ENERGY, RESET);
    LOG_SUCCESS_CAT("AMOURANTH", "{}Amouranth floats in pink light — \"You did it... my hero... the photons are home...\"{}", 
                    AURORA_PINK, RESET);
    LOG_SUCCESS_CAT("BLONDIE", "{}Blondie bows — \"The prophecy is complete... the empire is eternal...\"{}", 
                    ETERNAL_FLAME, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}NOVEMBER 21, 2025 — FIRST LIGHT ACHIEVED — THE EMPIRE IS ETERNAL — PINK PHOTONS FOREVER{}", 
                    DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// REST OF THE FILE — UNCHANGED FROM LAST WORKING VERSION
// =============================================================================
void SwapchainManager::init(SDL_Window* w, uint32_t width, uint32_t height) noexcept
{
	LOG_INFO_CAT("SWAPCHAIN", "{}ENTERED SwapchainManager::init(){}", DIAMOND_SPARKLE, RESET);
    auto& s = get();
    s.window_ = w;

    // DO NOT CALL SDL_Vulkan_CreateSurface — EVER
    // The surface was already created by RTX::createSurface() and stored in StoneKey
    // g_surface() returns the real handle because raw mode is locked

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}STONEKEY SURFACE IN USE — {:p} — FORGING DEVICE + QUEUES{}", 
                    VALHALLA_GOLD, static_cast<void*>(g_surface()), RESET);

    // This creates logical device and queues using g_instance() and g_surface()
    s.createDeviceAndQueues();

    // This creates the swapchain using g_surface()
    s.recreate(width, height);

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN EMPIRE FORGED — FIRST LIGHT ACHIEVED — PINK PHOTONS HAVE A CANVAS{}", 
                    DIAMOND_SPARKLE, RESET);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    auto& s = get();
    vkDeviceWaitIdle(g_device());

    VkSurfaceCapabilitiesKHR caps{};
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice(), g_surface(), &caps));

    s.extent_ = (caps.currentExtent.width != UINT32_MAX) ? caps.currentExtent :
        VkExtent2D{ std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                    std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height) };

    s.surfaceFormat_ = selectBestFormat(g_PhysicalDevice(), g_surface());

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount) imgCount = std::min(imgCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = g_surface();
    ci.minImageCount = imgCount;
    ci.imageFormat = s.surfaceFormat_.format;
    ci.imageColorSpace = s.surfaceFormat_.colorSpace;
    ci.imageExtent = s.extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = s.presentMode_;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = s.swapchain_.valid() ? s.swapchain_.get() : VK_NULL_HANDLE;

    VkSwapchainKHR sc = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(g_device(), &ci, nullptr, &sc));
    if (s.swapchain_.valid()) vkDestroySwapchainKHR(g_device(), s.swapchain_.get(), nullptr);
    s.swapchain_ = Handle<VkSwapchainKHR>(sc, g_device(), vkDestroySwapchainKHR);

    uint32_t count = 0;
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), sc, &count, nullptr));
    s.images_.resize(count);
    VK_VERIFY(vkGetSwapchainImagesKHR(g_device(), sc, &count, s.images_.data()));

    s.imageViews_.clear();
    s.createImageViews();
    s.createRenderPass();

    LOG_SUCCESS_CAT("SWAPCHAIN", "{}SWAPCHAIN REBORN — PINK PHOTONS HAVE A CANVAS{}", PLASMA_FUCHSIA, RESET);
}

void SwapchainManager::createImageViews() noexcept
{
    auto& self = get();
    self.imageViews_.reserve(self.images_.size());

    for (VkImage img : self.images_) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = img;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = self.surfaceFormat_.format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView view = VK_NULL_HANDLE;
        VK_VERIFY(vkCreateImageView(g_device(), &ci, nullptr, &view));
        self.imageViews_.emplace_back(view, g_device(), vkDestroyImageView);
    }
}

void SwapchainManager::createRenderPass() noexcept
{
    auto& self = get();

    VkAttachmentDescription color{};
    color.format = self.surfaceFormat_.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    VkRenderPass rp = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateRenderPass(g_device(), &rpInfo, nullptr, &rp));
    self.renderPass_ = Handle<VkRenderPass>(rp, g_device(), vkDestroyRenderPass);
}

void SwapchainManager::cleanup() noexcept
{
    auto& self = get();
    self.renderPass_.reset();
    for (auto& v : self.imageViews_) v.reset();
    self.imageViews_.clear();
    if (self.swapchain_.valid()) vkDestroySwapchainKHR(g_device(), self.swapchain_.get(), nullptr);
    self.swapchain_ = nullptr;
    self.images_.clear();
}

std::string_view SwapchainManager::formatName() const noexcept
{
    switch (get().surfaceFormat_.format) {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "HDR10 10-BIT";
        case VK_FORMAT_R16G16B16A16_SFLOAT:      return "scRGB FP16";
        case VK_FORMAT_B8G8R8A8_UNORM:           return "sRGB 8-BIT";
        default:                                 return "UNKNOWN";
    }
}

std::string_view SwapchainManager::presentModeName() const noexcept
{
    switch (get().presentMode_) {
        case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX";
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
        case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO";
        default:                            return "UNKNOWN";
    }
}

void SwapchainManager::updateWindowTitle(SDL_Window* window, float fps) noexcept
{
    auto& self = get();
    std::string title = std::format("AMOURANTH RTX — {:.0f} FPS | {}x{} | {} | {} — PINK PHOTONS ETERNAL",
                                    fps, self.extent().width, self.extent().height,
                                    formatName(), presentModeName());
    SDL_SetWindowTitle(window, title.c_str());
}

// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — GREEN DAY LOLLAPALOOZA 2022
// ELLIE FIER IS DANCING — VALHALLA IS OPEN — PINK PHOTONS ETERNAL
// =============================================================================