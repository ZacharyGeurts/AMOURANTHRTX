// src/engine/Vulkan/VulkanRenderer.cpp
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include "engine/camera.hpp"
#include "engine/utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <chrono>
#include <bit>
#include <thread>
#include <cstdint>
#include <atomic>
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#include <fstream>
#include <vulkan/vulkan_core.h>

using namespace Logging::Color;
using namespace Dispose;

namespace VulkanRTX {

// Helper
static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("Failed to find memory type");
}

/* -----------------------------------------------------------------
   VulkanRenderer::VulkanRenderer – FULL FINAL VERSION
   FIXED: Descriptor set not allocated!
   ORDER: RTX → POOL+SET (internal) → PIPELINE → SBT → COMMAND BUFFERS
   ----------------------------------------------------------------- */
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               std::shared_ptr<Vulkan::Context> context,
                               VulkanPipelineManager* pipelineManager,
                               VulkanBufferManager* bufferManager)
    : window_(window),
      context_(std::move(context)),
      pipelineManager_(pipelineManager),
      bufferManager_(bufferManager),
      width_(width),
      height_(height),
      lastFPSTime_(std::chrono::steady_clock::now()),
      currentRTIndex_(0),
      currentFrame_(0),
      frameCount_(0),
      framesThisSecond_(0),
      currentMode_(1),
      frameNumber_(0),
      currentAccumIndex_(0),
      envMapImage_(context_->device, VK_NULL_HANDLE, "EnvMap Image"),
      envMapImageMemory_(context_->device, VK_NULL_HANDLE, "EnvMap Memory"),
      envMapImageView_(context_->device, VK_NULL_HANDLE, "EnvMap View"),
      envMapSampler_(context_->device, VK_NULL_HANDLE, "EnvMap Sampler"),
      rtOutputImages_{
          Dispose::VulkanHandle<VkImage>(context_->device, VK_NULL_HANDLE, "RT Output 0"),
          Dispose::VulkanHandle<VkImage>(context_->device, VK_NULL_HANDLE, "RT Output 1")
      },
      rtOutputMemories_{
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "RT Mem 0"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "RT Mem 1")
      },
      rtOutputViews_{
          Dispose::VulkanHandle<VkImageView>(context_->device, VK_NULL_HANDLE, "RT View 0"),
          Dispose::VulkanHandle<VkImageView>(context_->device, VK_NULL_HANDLE, "RT View 1")
      },
      accumImages_{
          Dispose::VulkanHandle<VkImage>(context_->device, VK_NULL_HANDLE, "Accum Image 0"),
          Dispose::VulkanHandle<VkImage>(context_->device, VK_NULL_HANDLE, "Accum Image 1")
      },
      accumMemories_{
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Accum Mem 0"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Accum Mem 1")
      },
      accumViews_{
          Dispose::VulkanHandle<VkImageView>(context_->device, VK_NULL_HANDLE, "Accum View 0"),
          Dispose::VulkanHandle<VkImageView>(context_->device, VK_NULL_HANDLE, "Accum View 1")
      },
      commandBuffers_(MAX_FRAMES_IN_FLIGHT),
      descriptorPool_(context_->device, VK_NULL_HANDLE, "Renderer Desc Pool"),
      rtxDescriptorSet_(VK_NULL_HANDLE),
      computeDescriptorSets_{},
      uniformBuffers_{
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "UBO 0"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "UBO 1"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "UBO 2")
      },
      uniformBufferMemories_{
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "UBO Mem 0"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "UBO Mem 1"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "UBO Mem 2")
      },
      materialBuffers_{
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Mat Buf 0"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Mat Buf 1"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Mat Buf 2")
      },
      materialBufferMemory_{
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Mat Mem 0"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Mat Mem 1"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Mat Mem 2")
      },
      dimensionBuffers_{
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Dim Buf 0"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Dim Buf 1"),
          Dispose::VulkanHandle<VkBuffer>(context_->device, VK_NULL_HANDLE, "Dim Buf 2")
      },
      dimensionBufferMemory_{
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Dim Mem 0"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Dim Mem 1"),
          Dispose::VulkanHandle<VkDeviceMemory>(context_->device, VK_NULL_HANDLE, "Dim Mem 2")
      },
      tlasHandle_(VK_NULL_HANDLE),
      rtPipeline_(VK_NULL_HANDLE),
      rtPipelineLayout_(VK_NULL_HANDLE),
      swapchainRecreating_(false),
      queryReady_{false},
      framebuffers_(),
      queryPools_(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE),
      descriptorsUpdated_(false)
{
    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer::VulkanRenderer() — START [{}x{}]{}", 
                 OCEAN_TEAL, width, height, RESET);

    LOG_INFO_CAT("RENDERER", "  → context_ @ {}", static_cast<void*>(context_.get()), RESET);
    LOG_INFO_CAT("RENDERER", "  → device @ {}", static_cast<void*>(context_->device), RESET);
    LOG_INFO_CAT("RENDERER", "  → pipelineManager_ @ {}", static_cast<void*>(pipelineManager_), RESET);
    LOG_INFO_CAT("RENDERER", "  → bufferManager_ @ {}", static_cast<void*>(bufferManager_), RESET);

    if (!context_)               throw std::runtime_error("Null context");
    if (!context_->device)       throw std::runtime_error("Context has null VkDevice");
    if (!pipelineManager_)       throw std::runtime_error("Null pipeline manager");
    if (!bufferManager_)         throw std::runtime_error("Null buffer manager");

    // ───── LOAD DEFAULT CUBE MESH ─────
    LOG_INFO_CAT("RENDERER", "Loading default cube mesh...", OCEAN_TEAL, RESET);
    vertices_ = {
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
    };

    indices_ = {
        0,1,3, 3,1,2,  1,5,2,  2,5,6,  5,4,6,  6,4,7,
        4,0,7,  7,0,3,  3,2,7,  7,2,6,  4,5,0,  0,5,1
    };

    LOG_INFO_CAT("RENDERER", "Default cube mesh loaded: {} verts, {} indices", OCEAN_TEAL,
                 vertices_.size(), indices_.size(), RESET);

    // ───── CREATE SYNCHRONIZATION PRIMITIVES ─────
    LOG_INFO_CAT("RENDERER", "Creating semaphores and fences...", OCEAN_TEAL, RESET);
    VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]), "Image avail sem");
        VK_CHECK(vkCreateSemaphore(context_->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]), "Render fin sem");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]), "In-flight fence");
    }

    // ───── CREATE SWAPCHAIN (FPS UNLOCKED) ─────
    LOG_INFO_CAT("RENDERER", "Creating swapchain...", OCEAN_TEAL, RESET);
    createSwapchain();
    LOG_INFO_CAT("RENDERER", "Swapchain created: {}x{}, {} images", 
                 swapchainExtent_.width, swapchainExtent_.height, swapchainImages_.size(), RESET);

    // ───── QUERY POOLS ─────
    LOG_INFO_CAT("RENDERER", "Creating timestamp query pools...", OCEAN_TEAL, RESET);
    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo qi{
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2
        };
        VK_CHECK(vkCreateQueryPool(context_->device, &qi, nullptr, &pool), "Create query pool");
    }
    queryReady_.fill(false);

    // ───── COMPUTE DESCRIPTOR SET LAYOUT ─────
    LOG_INFO_CAT("RENDERER", "Fetching compute descriptor set layout...", OCEAN_TEAL, RESET);
    computeDescriptorSetLayout_ = pipelineManager_->getComputeDescriptorSetLayout();
    if (computeDescriptorSetLayout_ == VK_NULL_HANDLE)
        throw std::runtime_error("Compute descriptor set layout missing");
    LOG_INFO_CAT("RENDERER", "  → computeDescriptorSetLayout_ @ {}", 
                 static_cast<void*>(computeDescriptorSetLayout_), RESET);

    // ───── DESCRIPTOR POOL (FOR COMPUTE & UBOs) ─────
    LOG_INFO_CAT("RENDERER", "Creating renderer descriptor pool...", OCEAN_TEAL, RESET);
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 2 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT + 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &rawPool),
             "Failed to create descriptor pool");
    descriptorPool_ = makeHandle(context_->device, rawPool, "Renderer Desc Pool");
    LOG_INFO_CAT("RENDERER", "  → descriptorPool_ @ {}", static_cast<void*>(descriptorPool_.get()), RESET);

    // ───── RTX COMPONENT (OWNS ITS OWN POOL + SET) ─────
    LOG_INFO_CAT("RENDERER", "{}CREATING VulkanRTX COMPONENT...{}", EMERALD_GREEN, RESET);
    rtx_ = std::make_unique<VulkanRTX>(context_, swapchainExtent_.width, swapchainExtent_.height, pipelineManager_);
    LOG_INFO_CAT("RENDERER", "  → rtx_ @ {}", static_cast<void*>(rtx_.get()), RESET);

    // ← GET DESCRIPTOR SET FROM VulkanRTX (ALREADY ALLOCATED)
    LOG_INFO_CAT("RENDERER", "Fetching RTX descriptor set...", OCEAN_TEAL, RESET);
    rtxDescriptorSet_ = rtx_->getDescriptorSet();
    LOG_INFO_CAT("RENDERER", "  → rtxDescriptorSet_ @ {}", static_cast<void*>(rtxDescriptorSet_), RESET);

    if (rtxDescriptorSet_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "{}RTX FAILED TO ALLOCATE DESCRIPTOR SET!{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("RTX descriptor set missing");
    }

    LOG_INFO_CAT("RENDERER", "RTX descriptor set valid @ {}", static_cast<void*>(rtxDescriptorSet_), RESET);

    createComputeDescriptorSets();
    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();
    createEnvironmentMap();

    // ───── BUILD ACCELERATION STRUCTURES (CRITICAL) ─────
    LOG_INFO_CAT("RENDERER", "Building acceleration structures...", OCEAN_TEAL, RESET);
    buildAccelerationStructures();  // ← Uses default cube, builds TLAS

    // ───── PER-FRAME BUFFERS ─────
    LOG_INFO_CAT("RENDERER", "Initializing per-frame buffers...", OCEAN_TEAL, RESET);
    constexpr size_t kMaxMaterials = 256;
    constexpr VkDeviceSize kMaterialSize = kMaxMaterials * sizeof(MaterialData);
    constexpr VkDeviceSize kDimensionSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMaterialSize, kDimensionSize);

    // ───── RAY TRACING PIPELINE – MUST BE BEFORE SBT ─────
    LOG_INFO_CAT("RENDERER", "{}Creating ray tracing pipeline...{}", OCEAN_TEAL, RESET);
    pipelineManager_->createRayTracingPipeline();  // ← 1. CREATE PIPELINE FIRST

    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();

    if (rtPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Ray tracing pipeline creation failed!", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    LOG_INFO_CAT("RENDERER", "Ray tracing pipeline created @ {}", 
                 static_cast<void*>(rtPipeline_), RESET);

    // ───── PASS PIPELINE TO VulkanRTX – CRITICAL FIX ─────
    LOG_INFO_CAT("RENDERER", "Passing pipeline to VulkanRTX...", OCEAN_TEAL, RESET);
    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);

    // ───── SHADER BINDING TABLE (SBT) – NOW SAFE ─────
    LOG_INFO_CAT("RENDERER", "{}Creating Shader Binding Table...{}", OCEAN_TEAL, RESET);
    rtx_->createShaderBindingTable(context_->physicalDevice);  // ← 2. THEN SBT
    LOG_INFO_CAT("RENDERER", "SBT created successfully", EMERALD_GREEN, RESET);

    createCommandBuffers();

    // ───── UPDATE DESCRIPTORS (TLAS now valid) ─────
    LOG_INFO_CAT("RENDERER", "Updating descriptors...", OCEAN_TEAL, RESET);
    updateRTDescriptors();
    updateComputeDescriptors(0);

    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer initialized successfully{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   Destructor – RAII cleans everything
   ----------------------------------------------------------------- */
VulkanRenderer::~VulkanRenderer() {
    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer::~dtor RAII cleanup{}", EMERALD_GREEN, RESET);
    // All Dispose::VulkanHandle objects auto-destruct
}

/* -----------------------------------------------------------------
   createSwapchain
   ----------------------------------------------------------------- */
void VulkanRenderer::createSwapchain() {
    LOG_INFO_CAT("SWAPCHAIN", "{}createSwapchain() – START{}", OCEAN_TEAL, RESET);

    // --- Query surface capabilities ---
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice, context_->surface, &caps),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    // --- Query formats ---
    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, context_->surface, &formatCount, nullptr),
             "vkGetPhysicalDeviceSurfaceFormatsKHR count");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, context_->surface, &formatCount, formats.data()),
             "vkGetPhysicalDeviceSurfaceFormatsKHR");

    // --- Query present modes ---
    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, context_->surface, &presentModeCount, nullptr),
             "vkGetPhysicalDeviceSurfacePresentModesKHR count");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, context_->surface, &presentModeCount, presentModes.data()),
             "vkGetPhysicalDeviceSurfacePresentModesKHR");

    if (formatCount == 0 || presentModeCount == 0) {
        throw std::runtime_error("No surface formats or present modes");
    }

    // --- Choose format ---
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    // --- Choose present mode (FPS UNLOCKED) ---
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    for (const auto& m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = m;
            break;
        }
    }

    // --- Extent ---
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(static_cast<uint32_t>(width_), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height_), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // --- Image count ---
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    // --- Create swapchain ---
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context_->surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = swapchain_
    };

    VkSwapchainKHR newSwapchain;
    VK_CHECK(vkCreateSwapchainKHR(context_->device, &createInfo, nullptr, &newSwapchain),
             "vkCreateSwapchainKHR");

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
    }
    swapchain_ = newSwapchain;
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    // --- Get images ---
    uint32_t imgCount;
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, nullptr), "Get image count");
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(context_->device, swapchain_, &imgCount, swapchainImages_.data()), "Get images");

    // --- Create image views ---
    swapchainImageViews_.clear();
    swapchainImageViews_.reserve(imgCount);
    for (VkImage img : swapchainImages_) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &view), "Create image view");
        swapchainImageViews_.push_back(view);
    }

    LOG_INFO_CAT("SWAPCHAIN", "{}Swapchain created: {}x{}, {} images, {} mode{}", EMERALD_GREEN,
                 extent.width, extent.height, imgCount,
                 presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "MAILBOX", RESET);
}

/* -----------------------------------------------------------------
   cleanupSwapchain
   ----------------------------------------------------------------- */
void VulkanRenderer::cleanupSwapchain() {
    LOG_DEBUG_CAT("SWAPCHAIN", "{}Cleaning swapchain...{}", OCEAN_TEAL, RESET);
    for (auto fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(context_->device, fb, nullptr);
    }
    framebuffers_.clear();

    for (auto view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(context_->device, view, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

/* -----------------------------------------------------------------
   recreateSwapchain
   ----------------------------------------------------------------- */
void VulkanRenderer::recreateSwapchain(int width, int height) {
    width_ = width; height_ = height;
    vkDeviceWaitIdle(context_->device);
    cleanupSwapchain();
    createSwapchain();
    createFramebuffers();
    createCommandBuffers();
}

/* -----------------------------------------------------------------
   handleResize
   ----------------------------------------------------------------- */
void VulkanRenderer::handleResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    LOG_INFO_CAT("RESIZE", "{}Resize: {}x{} → {}x{}{}", AMBER_YELLOW, width_, height_, width, height, RESET);
    recreateSwapchain(width, height);
    if (rtx_) rtx_->updateRTX(context_->physicalDevice, context_->commandPool, context_->graphicsQueue, {}, {});
}

/* -----------------------------------------------------------------
   renderFrame – main render loop entry point
   ----------------------------------------------------------------- */
void VulkanRenderer::renderFrame(const Camera& camera) {
    LOG_DEBUG_CAT("FRAME", "{}renderFrame() – START (frame: {}){}", OCEAN_TEAL, frameNumber_, RESET);

    vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_->device, swapchain_, UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_INFO_CAT("SWAPCHAIN", "{}Out of date – recreating{}", AMBER_YELLOW, RESET);
        recreateSwapchain(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]);

    // --- Update per-frame data ---
    updateUniformBuffer(currentFrame_, camera);
    updateRTDescriptors();
    updateComputeDescriptors(currentFrame_);

    // --- Record command buffer ---
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

    // --- RAY TRACING PASS ---
    {
        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = rtOutputImages_[currentRTIndex_].get(),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        rtx_->recordRayTracingCommands(cmd, swapchainExtent_,
                                       rtOutputImages_[currentRTIndex_].get(),
                                       rtOutputViews_[currentRTIndex_].get());

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // --- COMPUTE DENOISE (mode 2) ---
    if (currentMode_ == 2) {
        VkImageMemoryBarrier rtToCompute{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = rtOutputImages_[currentRTIndex_].get(),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &rtToCompute);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineManager_->getComputePipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelineManager_->getComputePipelineLayout(),
                                0, 1, &computeDescriptorSets_[currentFrame_], 0, nullptr);

        vkCmdDispatch(cmd,
                      (swapchainExtent_.width + 15) / 16,
                      (swapchainExtent_.height + 15) / 16,
                      1);
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // --- Submit ---
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };

    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submitInfo, inFlightFences_[currentFrame_]),
             "vkQueueSubmit");

    // --- Present ---
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex
    };

    result = vkQueuePresentKHR(context_->graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(width_, height_);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    // --- Frame bookkeeping ---
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    frameCount_++;
    frameNumber_++;

    currentRTIndex_ = (currentRTIndex_ + 1) % 2;
    currentAccumIndex_ = (currentAccumIndex_ + 1) % 2;

    // --- CAMERA MOVEMENT DETECTION (NO getRotation()) ---
    static glm::vec3 lastCamPos = glm::vec3(0.0f);
    static float lastYaw = -90.0f;
    static float lastPitch = 0.0f;

    // Cast to PerspectiveCamera to access yaw_ and pitch_
    const PerspectiveCamera* persCam = dynamic_cast<const PerspectiveCamera*>(&camera);
    if (!persCam) {
        LOG_ERROR_CAT("CAM", "{}Camera is not PerspectiveCamera – cannot detect movement{}", CRIMSON_MAGENTA, RESET);
    } else {
        glm::vec3 currPos = persCam->getPosition();
        float currYaw = persCam->yaw_;
        float currPitch = persCam->pitch_;

        bool posMoved = glm::length(currPos - lastCamPos) > 0.001f;
        bool rotMoved = std::abs(currYaw - lastYaw) > 0.1f || std::abs(currPitch - lastPitch) > 0.1f;

        if (posMoved || rotMoved) {
            resetAccumulation_ = true;
            LOG_INFO_CAT("RTX", "{}Camera moved (pos: {:.3f}, yaw: {:.1f}, pitch: {:.1f}) – accumulation reset{}",
                         OCEAN_TEAL, glm::length(currPos - lastCamPos), currYaw - lastYaw, currPitch - lastPitch, RESET);

            lastCamPos = currPos;
            lastYaw = currYaw;
            lastPitch = currPitch;
        }
    }

    // --- FPS counter ---
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count();
    if (elapsed >= 1) {
        LOG_INFO_CAT("FPS", "{}FPS: {} | Frame: {} | Mode: {}{}",
                     OCEAN_TEAL, framesThisSecond_, frameNumber_, currentMode_, RESET);
        framesThisSecond_ = 0;
        lastFPSTime_ = now;
    } else {
        ++framesThisSecond_;
    }

    LOG_DEBUG_CAT("FRAME", "{}renderFrame() – END{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::createFramebuffers() {
    LOG_INFO_CAT("FB", "{}createFramebuffers() – START ({} images){}", OCEAN_TEAL, swapchainImageViews_.size(), RESET);

    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView attachments[] = { swapchainImageViews_[i] };

        VkFramebufferCreateInfo fbInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };

        VK_CHECK(vkCreateFramebuffer(context_->device, &fbInfo, nullptr, &framebuffers_[i]),
                 "vkCreateFramebuffer");
    }

    LOG_INFO_CAT("FB", "{}Created {} framebuffers{}", EMERALD_GREEN, framebuffers_.size(), RESET);
}

void VulkanRenderer::createRTOutputImages() {
    LOG_INFO_CAT("RTX", "{}createRTOutputImages() – START{}", OCEAN_TEAL, RESET);

    for (int i = 0; i < 2; ++i) {
        VkImage img = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;

        VkImageCreateInfo imgInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &img), "RT output image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(context_->device, img, &memReq);
        VkMemoryAllocateInfo alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &mem), "RT mem");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind RT");

        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &view), "RT view");

        rtOutputImages_[i]   = Dispose::makeHandle(context_->device, img,   "RT Output Image");
        rtOutputMemories_[i] = Dispose::makeHandle(context_->device, mem,  "RT Output Memory");
        rtOutputViews_[i]    = Dispose::makeHandle(context_->device, view, "RT Output View");

        VulkanInitializer::transitionImageLayout(*context_, img,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }

    LOG_INFO_CAT("RTX", "{}RT output images created{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::createAccumulationImages() {
    LOG_INFO_CAT("RTX", "{}createAccumulationImages() – START{}", OCEAN_TEAL, RESET);

    for (int i = 0; i < 2; ++i) {
        VkImage img = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;

        VkImageCreateInfo imgInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &img), "Accum image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(context_->device, img, &memReq);
        VkMemoryAllocateInfo alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &mem), "Accum mem");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind accum");

        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &view), "Accum view");

        accumImages_[i] = Dispose::makeHandle(context_->device, img, "Accum Image");
        accumMemories_[i] = Dispose::makeHandle(context_->device, mem, "Accum Memory");
        accumViews_[i] = Dispose::makeHandle(context_->device, view, "Accum View");

        VulkanInitializer::transitionImageLayout(*context_, img,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }

    LOG_INFO_CAT("RTX", "{}Accumulation images created{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::createComputeDescriptorSets() {
    LOG_INFO_CAT("DESC", "{}createComputeDescriptorSets() – START{}", OCEAN_TEAL, RESET);

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_.get(),
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };

    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> rawSets{};
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &alloc, rawSets.data()),
             "Allocate compute descriptor sets");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        computeDescriptorSets_[i] = rawSets[i];
    }

    LOG_INFO_CAT("DESC", "{}Compute descriptor sets allocated{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::updateRTDescriptors() {
    if (tlasHandle_ == VK_NULL_HANDLE || rtxDescriptorSet_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("DESC", "{}updateRTDescriptors() – TLAS or set missing{}", AMBER_YELLOW, RESET);
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle_
    };

    VkDescriptorImageInfo outInfo{
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[currentFrame_].get(),
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorBufferInfo matInfo{
        .buffer = materialBuffers_[currentFrame_].get(),
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorBufferInfo dimInfo{
        .buffer = dimensionBuffers_[currentFrame_].get(),
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorImageInfo envInfo{
        .sampler = envMapSampler_.get(),
        .imageView = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorImageInfo accumInfo{
        .imageView = accumViews_[currentAccumIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    std::array<VkWriteDescriptorSet, 7> writes{{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asWrite,
         .dstSet = rtxDescriptorSet_, .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 1, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 2, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 3, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 4, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 5, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 6, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo}
    }};

    vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    descriptorsUpdated_ = true;

    LOG_DEBUG_CAT("DESC", "{}RT descriptors updated{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::updateComputeDescriptors(uint32_t frameIdx) {
    VkDescriptorImageInfo rtInfo{
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo accumInfo{
        .imageView = accumViews_[currentAccumIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[frameIdx].get(),
        .range = VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 3> writes{{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &rtInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 1, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 2, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo}
    }};

    vkUpdateDescriptorSets(context_->device, 3, writes.data(), 0, nullptr);
}

void VulkanRenderer::updateComputeDescriptorSet(uint32_t frameIdx) {
    updateComputeDescriptors(frameIdx);
}

/* -----------------------------------------------------------------
   initializeAllBufferData – per-frame UBOs, Material, Dimension
   ----------------------------------------------------------------- */
void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames,
                                             VkDeviceSize materialSize,
                                             VkDeviceSize dimensionSize)
{
    LOG_INFO_CAT("BUFFERS", "{}initializeAllBufferData() – START ({} frames){}", OCEAN_TEAL, maxFrames, RESET);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        // --- UBO ---
        VkBuffer uboBuffer = VK_NULL_HANDLE;
        VkDeviceMemory uboMemory = VK_NULL_HANDLE;
        VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uboBuffer, uboMemory, nullptr, *context_);

        uniformBuffers_[i] = makeHandle(context_->device, uboBuffer, "UBO");
        uniformBufferMemories_[i] = makeHandle(context_->device, uboMemory, "UBO Memory");

        // --- Material SSBO ---
        VkBuffer matBuffer = VK_NULL_HANDLE;
        VkDeviceMemory matMemory = VK_NULL_HANDLE;
        VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
            materialSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            matBuffer, matMemory, nullptr, *context_);

        materialBuffers_[i] = makeHandle(context_->device, matBuffer, "Material SSBO");
        materialBufferMemory_[i] = makeHandle(context_->device, matMemory, "Material Memory");

        // --- Dimension SSBO ---
        VkBuffer dimBuffer = VK_NULL_HANDLE;
        VkDeviceMemory dimMemory = VK_NULL_HANDLE;
        VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
            dimensionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            dimBuffer, dimMemory, nullptr, *context_);

        dimensionBuffers_[i] = makeHandle(context_->device, dimBuffer, "Dimension SSBO");
        dimensionBufferMemory_[i] = makeHandle(context_->device, dimMemory, "Dimension Memory");
    }

    LOG_INFO_CAT("BUFFERS", "{}All per-frame buffers initialized{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::createEnvironmentMap() {
    LOG_INFO_CAT("ENV", "{}createEnvironmentMap() – START{}", OCEAN_TEAL, RESET);

    int w, h, c;
    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &c, 4);
    if (!pixels) {
        LOG_ERROR_CAT("ENV", "{}Failed to load envmap.hdr{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Failed to load environment map");
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VulkanBufferManager::createBuffer(
        context_->device, context_->physicalDevice,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory, nullptr, *context_);

    // Upload pixel data
    void* data;
    VK_CHECK(vkMapMemory(context_->device, stagingMemory, 0, imageSize, 0, &data), "Map staging");
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(context_->device, stagingMemory);
    stbi_image_free(pixels);

    // Create image
    VkImage envImage = VK_NULL_HANDLE;
    VkDeviceMemory envMemory = VK_NULL_HANDLE;
    VkImageView envView = VK_NULL_HANDLE;
    VkSampler envSampler = VK_NULL_HANDLE;

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &envImage), "Env image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context_->device, envImage, &memReq);
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(
            context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &envMemory), "Env mem");
    VK_CHECK(vkBindImageMemory(context_->device, envImage, envMemory, 0), "Bind env");

    // Transition to TRANSFER_DST
    VulkanInitializer::transitionImageLayout(*context_, envImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy buffer to image
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(context_->device, &cmdAlloc, &cmd), "Alloc env cmd");

    VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin env cmd");

    VkBufferImageCopy region{
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1}
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, envImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ
    VulkanInitializer::transitionImageLayout(*context_, envImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VK_CHECK(vkEndCommandBuffer(cmd), "End env cmd");

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, VK_NULL_HANDLE), "Submit env");
    vkQueueWaitIdle(context_->graphicsQueue);

    // Cleanup staging
    vkFreeCommandBuffers(context_->device, context_->commandPool, 1, &cmd);
    vkDestroyBuffer(context_->device, stagingBuffer, nullptr);
    vkFreeMemory(context_->device, stagingMemory, nullptr);

    // Create view
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = envImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &envView), "Env view");

    // Create sampler
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f
    };
    VK_CHECK(vkCreateSampler(context_->device, &samplerInfo, nullptr, &envSampler), "Env sampler");

    // Assign RAII handles
    envMapImage_ = Dispose::makeHandle(context_->device, envImage, "EnvMap Image");
    envMapImageMemory_ = Dispose::makeHandle(context_->device, envMemory, "EnvMap Memory");
    envMapImageView_ = Dispose::makeHandle(context_->device, envView, "EnvMap View");
    envMapSampler_ = Dispose::makeHandle(context_->device, envSampler, "EnvMap Sampler");

    LOG_INFO_CAT("ENV", "{}Environment map loaded: {}x{}{}", EMERALD_GREEN, w, h, RESET);
}

void VulkanRenderer::buildAccelerationStructures() {
    LOG_INFO_CAT("AS", "buildAccelerationStructures() – START", OCEAN_TEAL, RESET);

    if (vertices_.empty() || indices_.empty()) {
        LOG_ERROR_CAT("AS", "No mesh data! Using fallback cube.", CRIMSON_MAGENTA, RESET);
        // Still upload fallback cube if somehow empty
        vertices_ = {/* same as above */};
        indices_  = {/* same as above */};
    }

    LOG_INFO_CAT("AS", "Uploading mesh: {} verts, {} indices", OCEAN_TEAL,
                 vertices_.size(), indices_.size(), RESET);

    bufferManager_->uploadMesh(
        vertices_.data(), static_cast<uint32_t>(vertices_.size()),
        indices_.data(),  static_cast<uint32_t>(indices_.size())
    );

    VkBuffer vb = bufferManager_->getVertexBuffer();
    VkBuffer ib = bufferManager_->getIndexBuffer();

    if (vb == VK_NULL_HANDLE || ib == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("AS", "Failed to get buffers!", CRIMSON_MAGENTA, RESET);
        return;
    }

    LOG_INFO_CAT("AS", "Building BLAS + TLAS via PipelineManager...", OCEAN_TEAL, RESET);
    pipelineManager_->createAccelerationStructures(vb, ib);

    tlasHandle_ = pipelineManager_->getTLAS();
    if (tlasHandle_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("AS", "TLAS build failed!", CRIMSON_MAGENTA, RESET);
        return;
    }

    LOG_INFO_CAT("AS", "TLAS built: 0x{:x}", EMERALD_GREEN,
                 reinterpret_cast<uintptr_t>(tlasHandle_), RESET);

    // FORCE UPDATE DESCRIPTORS AFTER TLAS
    updateRTDescriptors();
    descriptorsUpdated_ = true;

    LOG_INFO_CAT("AS", "buildAccelerationStructures() – COMPLETE", OCEAN_TEAL, RESET);
}

/* -----------------------------------------------------------------
   updateDescriptorSetForTLAS
   ----------------------------------------------------------------- */
void VulkanRenderer::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (tlas == VK_NULL_HANDLE || rtxDescriptorSet_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("DESC", "{}updateDescriptorSetForTLAS() – null handle{}", AMBER_YELLOW, RESET);
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = rtxDescriptorSet_,
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_->device, 1, &write, 0, nullptr);
    tlasHandle_ = tlas;

    LOG_INFO_CAT("DESC", "{}TLAS descriptor updated: 0x{:x}{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(tlas), RESET);
}

/* -----------------------------------------------------------------
   updateGraphicsDescriptorSet
   ----------------------------------------------------------------- */
void VulkanRenderer::updateGraphicsDescriptorSet(uint32_t frameIndex) {
    // Placeholder – used for future raster passes
    LOG_DEBUG_CAT("DESC", "{}updateGraphicsDescriptorSet({}) – stub{}", OCEAN_TEAL, frameIndex, RESET);
}

/* -----------------------------------------------------------------
   createCommandBuffers
   ----------------------------------------------------------------- */
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainImageViews_.size());

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };

    VK_CHECK(vkAllocateCommandBuffers(context_->device, &allocInfo, commandBuffers_.data()),
             "Allocate command buffers");

    LOG_INFO_CAT("CMDBUF", "{}Allocated {} command buffers{}", EMERALD_GREEN, commandBuffers_.size(), RESET);
}

/* -----------------------------------------------------------------
   cleanup – manual cleanup for non-RAII resources
   ----------------------------------------------------------------- */
void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("CLEANUP", "{}VulkanRenderer::cleanup() – manual cleanup{}", CRIMSON_MAGENTA, RESET);

    vkDeviceWaitIdle(context_->device);

    // Query pools
    for (auto pool : queryPools_) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(context_->device, pool, nullptr);
        }
    }
    queryPools_.clear();

    // Semaphores & Fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (inFlightFences_[i]) vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }

    // Framebuffers (raw handles)
    for (auto fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(context_->device, fb, nullptr);
    }
    framebuffers_.clear();

    // Swapchain image views (raw handles)
    for (auto view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(context_->device, view, nullptr);
    }
    swapchainImageViews_.clear();

    // Swapchain
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    // RTX component
    rtx_.reset();

    LOG_INFO_CAT("CLEANUP", "{}Manual cleanup complete{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   applyResize – internal resize helper
   ----------------------------------------------------------------- */
void VulkanRenderer::applyResize(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) return;

    width_ = newWidth;
    height_ = newHeight;

    recreateSwapchain(newWidth, newHeight);
    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();

    resetAccumulation_ = true;
    descriptorsUpdated_ = false;

    LOG_INFO_CAT("RESIZE", "{}Applied resize: {}x{}{}", OCEAN_TEAL, newWidth, newHeight, RESET);
}

void VulkanRenderer::updateUniformBuffer(uint32_t frameIdx, const Camera& cam) {
    UniformBufferObject ubo{};
    ubo.viewInverse = glm::inverse(cam.getViewMatrix());
    ubo.projInverse = glm::inverse(cam.getProjectionMatrix());
    ubo.camPos = glm::vec4(cam.getPosition(), 1.0f);
    ubo.time = 0.0f;
    ubo.frame = frameCount_;

    void* data;
    VK_CHECK(vkMapMemory(context_->device,
                         uniformBufferMemories_[frameIdx].get(),
                         0, sizeof(ubo), 0, &data), "Map UBO");
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->device, uniformBufferMemories_[frameIdx].get());
}

/* -----------------------------------------------------------------
   dispatchRenderMode – stub (for future hybrid modes)
   ----------------------------------------------------------------- */
void VulkanRenderer::dispatchRenderMode(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer cmd,
    VkBuffer indexBuffer,
    float time,
    int width,
    int height,
    float fov,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkPipelineCache pipelineCache,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    const Vulkan::Context& context,
    int mode)
{
    LOG_WARN_CAT("RENDER", "{}dispatchRenderMode({}) – not implemented{}", AMBER_YELLOW, mode, RESET);
    // Future: raster, compute, hybrid
}

/* -----------------------------------------------------------------
   recreateRTOutputImage / recreateAccumulationImage
   ----------------------------------------------------------------- */
void VulkanRenderer::recreateRTOutputImage() {
    rtOutputImages_[currentRTIndex_].reset();
    rtOutputMemories_[currentRTIndex_].reset();
    rtOutputViews_[currentRTIndex_].reset();

    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &img), "Recreate RT output");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context_->device, img, &memReq);
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &mem), "Recreate RT mem");
    VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind RT");

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &view), "Recreate RT view");

    rtOutputImages_[currentRTIndex_]   = makeHandle(context_->device, img,   "RT Output Recreated");
    rtOutputMemories_[currentRTIndex_] = makeHandle(context_->device, mem,  "RT Mem Recreated");
    rtOutputViews_[currentRTIndex_]    = makeHandle(context_->device, view, "RT View Recreated");

    VulkanInitializer::transitionImageLayout(*context_, img,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void VulkanRenderer::recreateAccumulationImage() {
    accumImages_[currentAccumIndex_].reset();
    accumMemories_[currentAccumIndex_].reset();
    accumViews_[currentAccumIndex_].reset();

    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &img), "Recreate accum");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context_->device, img, &memReq);
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &mem), "Recreate accum mem");
    VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind accum");

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &view), "Recreate accum view");

    accumImages_[currentAccumIndex_] = makeHandle(context_->device, img, "Accum Recreated");
    accumMemories_[currentAccumIndex_] = makeHandle(context_->device, mem, "Accum Mem Recreated");
    accumViews_[currentAccumIndex_] = makeHandle(context_->device, view, "Accum View Recreated");

    VulkanInitializer::transitionImageLayout(*context_, img,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

const std::vector<glm::vec3>& VulkanRenderer::getVertices() const {
    return vertices_;
}

const std::vector<uint32_t>& VulkanRenderer::getIndices() const {
    return indices_;
}
} // namespace VulkanRTX