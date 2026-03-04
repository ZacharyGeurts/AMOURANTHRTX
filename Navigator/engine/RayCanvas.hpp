#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "camera.hpp"
#include "OptionsMenu.hpp"
#include "Pipeline.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <vector>
#include <cstdint>

// Extended Disney-style material (stable layout)
struct alignas(16) Material
{
    glm::vec4 baseColor         {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissive          {0.0f, 0.0f, 0.0f, 0.0f};

    float     metallic          = 0.0f;
    float     roughness         = 0.5f;
    float     specular          = 0.5f;
    float     ior               = 1.50f;

    float     transmission      = 0.0f;
    float     subsurface        = 0.0f;
    glm::vec3 subsurfaceColor   {0.8f,0.6f,0.5f};
    float     transmissionRoughness = 0.0f;

    float     clearcoat         = 0.0f;
    float     clearcoatRoughness= 0.03f;

    float     sheen             = 0.0f;
    glm::vec3 sheenTint         {1.0f,1.0f,1.0f};

    float     anisotropy        = 0.0f;
    float     anisoRotation     = 0.0f;

    float     thinFilm          = 0.0f;
    float     thinFilmIOR       = 1.45f;
    float     thinFilmThickness_nm = 350.0f;

    uint32_t  procType          = 0;
    float     procScale         = 8.0f;
    float     procStrength      = 0.35f;
    float     procOffsetSeed    = 0.0f;

    uint32_t  flags             = 0;
    uint32_t  padding[2]        = {0,0};
};

// Camera uniform block — includes previous position and matrices
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::mat4 prevView;
    glm::mat4 prevProj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    double    exposure     = 1.0;
    double    genesisTime  = 0.0;
    uint32_t  randomSeed   = 12345u;
    int       maxDepth     = Options::Rendering::MAX_RAY_RECURSION;

    uint32_t  padding[2]   = {0, 0};
};

// Living world — enhanced for volumetrics, fire, water caustics, etc.
struct LivingWorldData {
    glm::vec4 sunDirAndIntensity;
    glm::vec4 skyDayTop;
    glm::vec4 skyDayHorizon;
    glm::vec4 skyNight;
    glm::vec4 groundColorDay;
    glm::vec4 groundColorNight;
    float     dayNightFactor;
    float     cloudDensity;
    float     fogDensity;
    float     temperature;

    float     volumetricDensity      = 0.015f;
    float     volumetricAnisotropy   = 0.35f;
    float     volumetricEmissionScale = 1.0f;
    float     causticsStrength       = 0.0f;

    uint32_t  frameSeed;
    uint32_t  padding[2];
};

// =============================================================================
// RayCanvas — Persistent compute-based renderer with temporal support
// =============================================================================
class RayCanvas {
public:
    RayCanvas(int windowWidth, int windowHeight, SDL_Window* window)
        : window_(window),
          window_width_(windowWidth),
          window_height_(windowHeight),
          internal_width_(Options::Rendering::INTERNAL_WIDTH),
          internal_height_(Options::Rendering::INTERNAL_HEIGHT),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          materialsHandle_(0),
          primitivesHandle_(0),
          cameraUBOHandle_(0),
          livingWorldHandle_(0),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          prevHdrOutputImage_(VK_NULL_HANDLE),
          prevHdrOutputView_(VK_NULL_HANDLE),
          prevHdrOutputMemory_(VK_NULL_HANDLE),
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE)
    {
        Swapchain::create(window, window_width_, window_height_);

        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to create swapchain");
            std::abort();
        }

        cameraUBOHandle_ = Memory::createBuffer(
            sizeof(CameraSceneData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "CameraUBO"
        );

        livingWorldHandle_ = Memory::createBuffer(
            sizeof(LivingWorldData),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "LivingWorld",
            Memory::MemoryHint::HostVisible
        );

        // Material library with procedural variation
        std::vector<Material> sceneMaterials;
        auto addMat = [&](Material m) {
            if (m.transmission      > 0.001f) m.flags |= (1u << 0);
            if (m.subsurface        > 0.001f) m.flags |= (1u << 1);
            if (m.clearcoat         > 0.001f) m.flags |= (1u << 2);
            if (m.sheen             > 0.001f) m.flags |= (1u << 3);
            if (m.thinFilm          > 0.001f) m.flags |= (1u << 4);
            if (std::abs(m.anisotropy) > 0.001f) m.flags |= (1u << 5);
            sceneMaterials.push_back(m);
        };

        // Base materials (your original 6)
        {
            Material m{}; m.baseColor = {0.95f,0.64f,0.07f,1.0f}; m.metallic=1.0f; m.roughness=0.08f; m.ior=1.5f; addMat(m);
        }
        {
            Material m{}; m.baseColor = {0.04f,0.04f,0.04f,1.0f}; m.roughness=0.92f; m.ior=1.5f; m.sheen=0.95f; m.sheenTint={0.9f,0.9f,0.9f}; addMat(m);
        }
        {
            Material m{}; m.baseColor = {0.9f,0.9f,1.0f,0.4f}; m.roughness=0.02f; m.ior=1.45f; m.transmission=0.98f; addMat(m);
        }
        {
            Material m{}; m.baseColor = {1.0f,0.1f,0.4f,1.0f}; m.emissive={8.0f,1.0f,2.0f,15.0f}; m.roughness=0.4f; addMat(m);
        }
        {
            Material m{}; m.baseColor = {0.2f,0.7f,0.2f,1.0f}; m.roughness=0.85f; m.ior=1.5f; m.subsurface=0.4f; m.subsurfaceColor={0.3f,0.9f,0.4f}; addMat(m);
        }
        {
            Material m{}; m.baseColor = {0.9f,0.92f,1.0f,1.0f}; m.roughness=0.04f; m.ior=1.45f; m.thinFilm=1.0f; m.thinFilmThickness_nm=620.0f; addMat(m);
        }

        size_t baseCount = sceneMaterials.size();
        constexpr int TOTAL_MATERIALS = 8000;
        for (size_t i = baseCount; sceneMaterials.size() < TOTAL_MATERIALS; ++i) {
            size_t baseIdx = (i - baseCount) % baseCount;
            Material m = sceneMaterials[baseIdx];
            float rnd = hash11(static_cast<uint32_t>(i) * 214013u + 2531011u);

            if (rnd < 0.7f) {
                float hueShift = (rnd * 2.0f - 1.0f) * 0.45f;
                m.baseColor = hue_shift(m.baseColor, hueShift);
            }

            m.roughness = glm::clamp(m.roughness + (rnd - 0.5f) * 0.75f, 0.02f, 0.98f);

            if (rnd > 0.35f && rnd < 0.65f)
                m.metallic = glm::clamp(m.metallic + (rnd - 0.5f) * 0.9f, 0.0f, 1.0f);

            if (rnd > 0.38f && rnd < 0.82f) {
                m.procType     = 1 + (static_cast<uint32_t>(i * 7u) % 6);
                m.procScale    = 4.0f + rnd * 24.0f;
                m.procStrength = 0.15f + rnd * 0.65f;
                m.procOffsetSeed = rnd * 100.0f;
            }

            if (rnd < 0.12f) {
                m.emissive = glm::vec4(2.0f + rnd*10.0f, 0.1f+rnd*4.0f, 0.3f+rnd*6.0f, 8.0f + rnd*30.0f);
            }

            if (rnd > 0.88f) {
                m.thinFilm = 0.7f + rnd * 0.3f;
                m.thinFilmThickness_nm = 220.0f + (rnd * 980.0f);
            }

            addMat(m);
        }

        VkDeviceSize matSize = sceneMaterials.size() * sizeof(Material);
        materialsHandle_ = Memory::createBuffer(matSize,
                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                "MaterialsBuffer", Memory::MemoryHint::HostVisible);

        if (matSize > 0) {
            auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(materialsHandle_, sceneMaterials.data(), matSize);
            if (stagingBuf != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                vkFreeMemory(rtx().device, stagingMem, nullptr);
            }
        }

        VkDeviceSize primSize = rtx().las_procedural_primitives.size() * sizeof(UniversalPrimitive);
        if (primSize == 0) primSize = sizeof(UniversalPrimitive);

        primitivesHandle_ = Memory::createBuffer(primSize,
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 primSize == sizeof(UniversalPrimitive) ? "PrimitivesBuffer (dummy)" : "PrimitivesBuffer",
                                                 Memory::MemoryHint::HostVisible);

        if (primSize == sizeof(UniversalPrimitive)) {
            UniversalPrimitive dummy{};
            auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(primitivesHandle_, &dummy, primSize);
            if (stagingBuf != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                vkFreeMemory(rtx().device, stagingMem, nullptr);
            }
        } else {
            auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(primitivesHandle_, rtx().las_procedural_primitives.data(), primSize);
            if (stagingBuf != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                vkFreeMemory(rtx().device, stagingMem, nullptr);
            }
        }

        createPersistentHDR();
        createPreviousHDR();
        createDescriptorPoolAndSet();

        Pipeline::initialize();
        Pipeline::create_pipeline_layout();
        Pipeline::create_canvas_pipeline();

        updateDescriptorSet();
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        vkDeviceWaitIdle(rtx().device);

        vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
        vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);

        vkDestroyImageView (rtx().device, hdrOutputView_,   nullptr);
        vkDestroyImage     (rtx().device, hdrOutputImage_,  nullptr);
        vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        Memory::destroy(cameraUBOHandle_);
        Memory::destroy(livingWorldHandle_);
        Memory::destroy(materialsHandle_);
        Memory::destroy(primitivesHandle_);

        Pipeline::shutdown();
    }

void maybeUpdateCanvas() noexcept {
    if (destroyed_) return;

    int currentW = 0, currentH = 0;
    SDL_GetWindowSize(window_, &currentW, &currentH);

    bool sizeChanged   = (currentW != window_width_ || currentH != window_height_);
    bool wasMinimized  = minimized_;
    bool nowMinimized  = (currentW <= 0 || currentH <= 0);

    if (nowMinimized) {
        minimized_ = true;
        return;
    }

    if (sizeChanged || wasMinimized) {
        vkDeviceWaitIdle(rtx().device);
        onResize(currentW, currentH);
        minimized_ = false;
    }

    if (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_) {
        LOG_WARNING_CAT("RAYCANVAS", "Invalid state — skipping frame");
        return;
    }

    if (firstFrame_) {
        TotalTime::get().seal();
        firstFrame_ = false;
        LOG_AMOURANTH("Genesis sealed — eternal canvas begins 💖");
    }

    double now = TotalTime::get().seconds();
    static double lastKnownTime = 0.0;
    if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
    lastKnownTime = now;

    updateCameraUBO(now);
    updateLivingWorldBuffer(now);
    updateDescriptorSet();

    uint32_t imageIndex = 0;
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(rtx().device, &fci, nullptr, &fence);

    VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX,
                                                VK_NULL_HANDLE, fence, &imageIndex);

    vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(rtx().device, fence, nullptr);

    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
        LOG_WARNING_CAT("SWAPCHAIN", "Acquire out-of-date — retrying next frame");
        minimized_ = true;
        return;
    }

    if (acq != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
        if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) destroyed_ = true;
        return;
    }

    VkCommandBuffer cmd = beginTransientCommandBuffer();
    if (!cmd) return;

    // Ensure HDR is in GENERAL layout before compute writes
    transitionImageLayout(cmd, hdrOutputImage_,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    if (!firstFrame_) {
        copyHDRtoPrevious(cmd);
    }

    VkDescriptorSet set = descriptorSet_;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::pipeline_layout,
                            0, 1, &set, 0, nullptr);

    Pipeline::dispatch_canvas(cmd,
                              static_cast<uint32_t>(internal_width_),
                              static_cast<uint32_t>(internal_height_),
                              static_cast<float>(now));

    // Critical barrier: make compute writes visible to transfer/blit
    VkImageMemoryBarrier postComputeBarrier{};
    postComputeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postComputeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    postComputeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    postComputeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    postComputeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    postComputeBarrier.image = hdrOutputImage_;
    postComputeBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &postComputeBarrier);

    endSubmitAndWait(cmd);

    // Blit phase
    VkCommandBuffer blitCmd = beginTransientCommandBuffer();
    if (!blitCmd) return;

    VkImage swapImg = Swapchain::images[imageIndex];

    // Transition swapchain image to TRANSFER_DST
    VkImageMemoryBarrier swapBarrier{};
    swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.image = swapImg;
    swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(blitCmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

    // Optional: clear swapchain to black for debug (remove later)
    VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(blitCmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearBlack, 1, &range);

    Swapchain::scaleBlit(blitCmd,
                         hdrOutputImage_,
                         VkExtent2D{static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_)},
                         swapImg,
                         Swapchain::getExtent());

    // Transition swapchain back to PRESENT_SRC_KHR
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    presentBarrier.image = swapImg;
    presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(blitCmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    endSubmitAndWait(blitCmd);

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1;
    pi.pSwapchains    = &Swapchain::swapchain.value;
    pi.pImageIndices  = &imageIndex;

    VkResult pres = ext().vkQueuePresentKHR(rtx().present_queue, &pi);

    if (pres == VK_SUCCESS) {
        Swapchain::updateRefreshEstimate(now);
    } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        LOG_WARNING_CAT("SWAPCHAIN", "Present out-of-date — recreate next frame");
        minimized_ = true;
    } else if (pres != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
        if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
    }
}

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == window_width_ && newHeight == window_height_) return;

        vkDeviceWaitIdle(rtx().device);

        window_width_  = newWidth;
        window_height_ = newHeight;
        minimized_ = false;

        LOG_INFO_CAT("WINDOW", "Resizing to {}x{}", newWidth, newHeight);

        Swapchain::recreate(window_width_, window_height_);

        using namespace Options::GameStyle;
        if (CurrentDimension == DimensionMode::Pure2D || CurrentDimension == DimensionMode::TwoPointFiveD) {
            internal_width_  = std::min(1280, window_width_);
            internal_height_ = std::min(720, window_height_);
        } else {
            internal_width_  = Options::Rendering::INTERNAL_WIDTH;
            internal_height_ = Options::Rendering::INTERNAL_HEIGHT;
        }

        createPersistentHDR();
        createPreviousHDR();
        updateDescriptorSet();
    }

    int  getWidth()  const noexcept { return window_width_; }
    int  getHeight() const noexcept { return window_height_; }
    bool isMinimized() const noexcept { return minimized_; }
    bool isDestroyed() const noexcept { return destroyed_; }

private:
    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[5] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
        };

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pci.maxSets       = 1;
        pci.poolSizeCount = 5;
        pci.pPoolSizes    = poolSizes;

        vkh.checker(vkCreateDescriptorPool(rtx().device, &pci, nullptr, &descriptorPool_),
                    "vkCreateDescriptorPool", "RayCanvas");

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = descriptorPool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkh.checker(vkAllocateDescriptorSets(rtx().device, &ai, &descriptorSet_),
                    "vkAllocateDescriptorSets", "RayCanvas");
    }

    void updateDescriptorSet() noexcept {
        if (!hdrOutputView_) return;

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorBufferInfo uboInfo{ Memory::getBuffer(cameraUBOHandle_), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo worldInfo{ Memory::getBuffer(livingWorldHandle_), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo matInfo{ Memory::getBuffer(materialsHandle_), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo primInfo{ Memory::getBuffer(primitivesHandle_), 0, VK_WHOLE_SIZE };

        VkWriteDescriptorSet writes[5]{};

        writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet_, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &imgInfo,   nullptr, nullptr };
        writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet_, 1, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfo,  nullptr };
        writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet_, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &worldInfo,nullptr };
        writes[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet_, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &matInfo,  nullptr };
        writes[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet_, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &primInfo, nullptr };

        vkUpdateDescriptorSets(rtx().device, 5, writes, 0, nullptr);
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_)   vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        if (hdrOutputImage_)  vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        if (hdrOutputMemory_) vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_   = VK_NULL_HANDLE;
        hdrOutputImage_  = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_),
                    "vkCreateImage", "HDR");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, memType };

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_),
                    "vkAllocateMemory", "HDR");

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_),
                    "vkCreateImageView", "HDR");
    }

    void createPreviousHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (prevHdrOutputView_)   vkDestroyImageView(rtx().device, prevHdrOutputView_, nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage(rtx().device, prevHdrOutputImage_, nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory(rtx().device, prevHdrOutputMemory_, nullptr);

        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_),
                    "vkCreateImage", "Prev HDR");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, prevHdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, memType };

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &prevHdrOutputMemory_),
                    "vkAllocateMemory", "Prev HDR");

        vkBindImageMemory(rtx().device, prevHdrOutputImage_, prevHdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = prevHdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &prevHdrOutputView_),
                    "vkCreateImageView", "Prev HDR");
    }

    void copyHDRtoPrevious(VkCommandBuffer cmd) noexcept {
        if (!prevHdrOutputImage_ || !hdrOutputImage_) return;

        VkImageMemoryBarrier barriers[2]{};

        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].image = hdrOutputImage_;
        barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcAccessMask = 0;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].image = prevHdrOutputImage_;
        barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 2, barriers);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.srcOffset = {0,0,0};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstOffset = {0,0,0};
        copyRegion.extent = {static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_), 1};

        vkCmdCopyImage(cmd,
            hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            prevHdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion);

        VkImageMemoryBarrier postBarrier{};
        postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        postBarrier.image = prevHdrOutputImage_;
        postBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &postBarrier);
    }

    void updateLivingWorldBuffer(double now) noexcept {
        LivingWorldData data{};

        float dayLength = Options::Sky::DAY_LENGTH_SECONDS;
        float dayFrac   = fmodf(static_cast<float>(now), dayLength) / dayLength;
        float sunAngle  = dayFrac * 2.0f * glm::pi<float>() - glm::pi<float>() * 0.5f;

        glm::vec3 sunDir = glm::normalize(glm::vec3(
            cosf(sunAngle),
            sinf(sunAngle) * 0.8f + 0.2f,
            sinf(sunAngle * 1.5f) * 0.3f
        ));

        float sunHeight = sunDir.y;
        float dayFactor = glm::smoothstep(-0.1f, 0.3f, sunHeight);
        float intensity = glm::max(0.0f, sunHeight) * 4.0f + 0.1f;

        data.sunDirAndIntensity     = glm::vec4(sunDir, intensity);
        data.skyDayTop              = Options::Sky::SKY_ZENITH_DAY;
        data.skyDayHorizon          = Options::Sky::SKY_HORIZON_DAY;
        data.skyNight               = Options::Sky::SKY_ZENITH_NIGHT;
        data.groundColorDay         = Options::Sky::GROUND_COLOR_DAY;
        data.groundColorNight       = Options::Sky::GROUND_COLOR_NIGHT;
        data.dayNightFactor         = dayFactor;
        data.cloudDensity           = Options::Sky::CLOUD_DENSITY;
        data.fogDensity             = Options::Sky::FOG_DENSITY;
        data.temperature            = 20.0f + 10.0f * sinf(dayFrac * 2.0f * glm::pi<float>());

        data.volumetricDensity      = 0.015f;
        data.volumetricAnisotropy   = 0.35f;
        data.volumetricEmissionScale= 1.0f;
        data.causticsStrength       = 0.0f;

        data.frameSeed              = static_cast<uint32_t>(now * 1000.0f);

        auto* info = Memory::get(livingWorldHandle_);
        if (info && info->mapped) {
            std::memcpy(info->mapped, &data, sizeof(data));
        } else {
            auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(livingWorldHandle_, &data, sizeof(data));
            if (stagingBuf != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                vkFreeMemory(rtx().device, stagingMem, nullptr);
            }
        }
    }

    void updateCameraUBO(double now) noexcept {
        CameraSceneData data{};

        using namespace Options::GameStyle;
        if (CurrentPerspective == CameraPerspective::Orthographic2D ||
            CurrentPerspective == CameraPerspective::SideScroller) {
            float aspect = static_cast<float>(internal_width_) / static_cast<float>(internal_height_);
            data.proj = glm::ortho(-aspect * 5.0f, aspect * 5.0f, -5.0f, 5.0f, 0.1f, 1000.0f);
        } else {
            data.proj = CAM.projection(static_cast<float>(internal_width_) / static_cast<float>(internal_height_));
        }

        static glm::mat4 lastView = data.view;
        static glm::mat4 lastProj = data.proj;

        data.prevView = lastView;
        data.prevProj = lastProj;

        data.view        = CAM.view();
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos   = glm::vec4(CAM.position(), 1.0f);
        data.prevCameraPos = glm::vec4(CAM.prevPosition(), 1.0f);

        data.exposure    = Options::Rendering::EXPOSURE;
        data.genesisTime = now;
        data.randomSeed  = static_cast<uint32_t>(now * 1'000'000.0) ^ 0xCAFEBABEu;

        lastView = data.view;
        lastProj = data.proj;

        auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));
        if (stagingBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
            vkFreeMemory(rtx().device, stagingMem, nullptr);
        }
    }

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = rtx().transient_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(rtx().device, &alloc, &cmd) != VK_SUCCESS) return VK_NULL_HANDLE;

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return VK_NULL_HANDLE;
        }

        return cmd;
    }

    void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return;
        }

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (vkCreateFence(rtx().device, &fenceCI, nullptr, &fence) != VK_SUCCESS) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return;
        }

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        VkResult res = vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
        if (res != VK_SUCCESS) {
            vkDestroyFence(rtx().device, fence, nullptr);
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return;
        }

        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
    }

private:
    static glm::vec4 hue_shift(glm::vec4 c, float shift) {
        glm::vec3 rgb = glm::vec3(c);
        float u = cosf(shift * glm::pi<float>() * 2.0f);
        float w = sinf(shift * glm::pi<float>() * 2.0f);

        glm::mat3 rot = glm::mat3(
            0.299f + 0.701f * u + 0.168f * w,
            0.587f - 0.587f * u + 0.330f * w,
            0.114f - 0.114f * u - 0.497f * w,
            0.299f - 0.299f * u - 0.328f * w,
            0.587f + 0.413f * u + 0.035f * w,
            0.114f - 0.114f * u + 0.292f * w,
            0.299f - 0.300f * u + 1.250f * w,
            0.587f - 0.588f * u - 1.050f * w,
            0.114f + 0.886f * u - 0.203f * w
        );

        rgb = rot * rgb;
        return glm::vec4(glm::clamp(rgb, 0.0f, 1.0f), c.a);
    }

    static float hash11(uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return float(x) * (1.0f / 4294967296.0f);
    }

private:
    SDL_Window*    window_           = nullptr;
    int            window_width_     = 0;
    int            window_height_    = 0;
    int            internal_width_   = 0;
    int            internal_height_  = 0;
    bool           minimized_        = false;
    bool           destroyed_        = false;
    bool           firstFrame_       = true;

    uint64_t       materialsHandle_  = 0;
    uint64_t       primitivesHandle_ = 0;
    uint64_t       cameraUBOHandle_  = 0;
    uint64_t       livingWorldHandle_= 0;

    VkImage        hdrOutputImage_   = VK_NULL_HANDLE;
    VkImageView    hdrOutputView_    = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_  = VK_NULL_HANDLE;

    VkImage        prevHdrOutputImage_   = VK_NULL_HANDLE;
    VkImageView    prevHdrOutputView_    = VK_NULL_HANDLE;
    VkDeviceMemory prevHdrOutputMemory_  = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_  = VK_NULL_HANDLE;
};