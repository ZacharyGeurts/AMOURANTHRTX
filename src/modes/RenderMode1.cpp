// src/modes/RenderMode1.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — GLOBAL MONSOON — PURE RTX — scene.obj LOADED
// =============================================================================

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <tiny_obj_loader.h>
#include <filesystem>

using namespace Logging::Color;

extern RTX::PipelineManager* g_pipeline_manager;  // ← Exists in your engine

static std::string findAsset(const std::string& rel) {
    std::vector<std::string> paths = {
        rel, "assets/" + rel, "../assets/" + rel
    };
    char exe[1024];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe)-1);
    if (len != -1) {
        exe[len] = '\0';
        paths.push_back((std::filesystem::path(exe).parent_path() / "assets" / rel).string());
    }
    for (const auto& p : paths) {
        if (std::filesystem::exists(p)) return p;
    }
    return "";
}

void RenderMode1::loadSceneFromDisk() {
    if (sceneLoaded_) return;

    std::string path = findAsset("models/scene.obj");
    if (path.empty()) {
        LOG_WARN_CAT("RTX", "scene.obj not found — pink void reigns supreme");
        return;
    }

    LOG_SUCCESS_CAT("RTX", "scene.obj FOUND → {}", path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                path.c_str(),
                                std::filesystem::path(path).parent_path().string().c_str());

    if (!warn.empty()) LOG_WARN_CAT("RTX", "tinyobj: {}", warn);
    if (!err.empty()) LOG_ERROR_CAT("RTX", "tinyobj: {}", err);
    if (!ret) return;

    LOG_SUCCESS_CAT("RTX", "scene.obj LOADED — {} verts, {} tris — PHOTONS INCOMING",
                    attrib.vertices.size()/3, shapes[0].mesh.indices.size()/3);

    g_rtx().buildAccelerationStructures();
    sceneLoaded_ = true;

    LOG_SUCCESS_CAT("RTX", "GLOBAL MONSOON ACTIVE — PURE RTX PATH TRACING — PINK PHOTONS ETERNAL");
}

RenderMode1::RenderMode1(uint32_t width, uint32_t height)
    : width_(width), height_(height), lastFrame_(std::chrono::steady_clock::now()) {

    LOG_INFO_CAT("RTX", "MODE 1 — PURE RTX PATH TRACING — {}×{}", width, height);
    initResources();
    loadSceneFromDisk();
}

RenderMode1::~RenderMode1() {
    cleanupResources();
}

void RenderMode1::initResources() {
    cleanupResources();

    VkDeviceSize uboSize = sizeof(glm::mat4) + sizeof(float) + sizeof(uint32_t);
    BUFFER_CREATE(uniformBuf_, uboSize,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Mode1 UBO");

    accumSize_ = width_ * height_ * 16;
    BUFFER_CREATE(accumulationBuf_, accumSize_,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Mode1 Accum");

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Accum Image
    imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage rawImg; vkCreateImage(g_device(), &imgInfo, nullptr, &rawImg);
    accumImage_ = RTX::Handle<VkImage>(rawImg, g_device(), vkDestroyImage, 0, "AccumImg");

    VkMemoryRequirements memReqs; vkGetImageMemoryRequirements(g_device(), rawImg, &memReqs);
    uint32_t memType = g_pipeline_manager->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, memType};
    VkDeviceMemory mem; vkAllocateMemory(g_device(), &alloc, nullptr, &mem);
    vkBindImageMemory(g_device(), rawImg, mem, 0);
    accumMem_ = RTX::Handle<VkDeviceMemory>(mem, g_device(), vkFreeMemory, memReqs.size, "AccumMem");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view; vkCreateImageView(g_device(), &viewInfo, nullptr, &view);
    accumView_ = RTX::Handle<VkImageView>(view, g_device(), vkDestroyImageView, 0, "AccumView");

    // Output Image
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vkCreateImage(g_device(), &imgInfo, nullptr, &rawImg);
    outputImage_ = RTX::Handle<VkImage>(rawImg, g_device(), vkDestroyImage, 0, "OutputImg");

    vkGetImageMemoryRequirements(g_device(), rawImg, &memReqs);
    alloc.allocationSize = memReqs.size;
    vkAllocateMemory(g_device(), &alloc, nullptr, &mem);
    vkBindImageMemory(g_device(), rawImg, mem, 0);
    outputMem_ = RTX::Handle<VkDeviceMemory>(mem, g_device(), vkFreeMemory, memReqs.size, "OutputMem");

    viewInfo.image = rawImg;
    viewInfo.format = imgInfo.format;
    vkCreateImageView(g_device(), &viewInfo, nullptr, &view);
    outputView_ = RTX::Handle<VkImageView>(view, g_device(), vkDestroyImageView, 0, "OutputView");

    // Transition
    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(RTX::g_ctx().commandPool());
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    barrier.image = accumImage_.get();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    barrier.image = outputImage_.get();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    g_pipeline_manager->endSingleTimeCommands(RTX::g_ctx().commandPool(), RTX::g_ctx().graphicsQueue(), cmd);

    g_rtx().updateRTXDescriptors(0,
        RAW_BUFFER(uniformBuf_), RAW_BUFFER(accumulationBuf_), VK_NULL_HANDLE,
        accumView_.get(), outputView_.get(), VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void RenderMode1::cleanupResources() {
    vkDeviceWaitIdle(g_device());
    g_rtx().updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                 VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                 VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    if (uniformBuf_) BUFFER_DESTROY(uniformBuf_);
    if (accumulationBuf_) BUFFER_DESTROY(accumulationBuf_);
    accumImage_.reset(); outputImage_.reset();
    accumView_.reset(); outputView_.reset();
    accumMem_.reset(); outputMem_.reset();
}

void RenderMode1::updateUniforms(float) {
    void* data = nullptr;
    BUFFER_MAP(uniformBuf_, data);
    if (data) {
        float aspect = float(width_) / height_;
        glm::mat4 vp = GlobalCamera::get().proj(aspect) * GlobalCamera::get().view();
        float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastFrame_).count();
        memcpy(data, glm::value_ptr(vp), sizeof(vp));
        memcpy((char*)data + sizeof(vp), &t, sizeof(t));
        memcpy((char*)data + sizeof(vp) + sizeof(t), &frameCount_, sizeof(frameCount_));
        BUFFER_UNMAP(uniformBuf_);
    }
}

void RenderMode1::traceRays(VkCommandBuffer cmd) {
    g_rtx().recordRayTrace(cmd, {width_, height_}, outputImage_.get(), outputView_.get());
}

void RenderMode1::accumulateAndToneMap(VkCommandBuffer cmd) {
    if (!sceneLoaded_) {
        VkClearColorValue pink{{1.0f, 0.3f, 0.8f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, outputImage_.get(), VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);
    }
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    updateUniforms(deltaTime);
    traceRays(cmd);
    accumulateAndToneMap(cmd);
    frameCount_++;
    lastFrame_ = std::chrono::steady_clock::now();
}

void RenderMode1::onResize(uint32_t width, uint32_t height) {
    if (width == width_ && height == height_) return;
    width_ = width; height_ = height;
    frameCount_ = 0;
    cleanupResources();
    initResources();
}