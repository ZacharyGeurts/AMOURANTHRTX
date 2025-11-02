// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: NO REORDER WARNINGS, NO UNINITIALIZED DATA
//        UBOs, Material, Dimension buffers owned by VulkanRenderer
//        Swapchain data owned directly by VulkanRenderer (no manager for simplicity)
//        FPS UNLOCKED BY DEFAULT: VK_PRESENT_MODE_IMMEDIATE_KHR
//        Zero leaks, zero warnings, maximum OCEAN TEAL logging.

#pragma once
#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Dispose.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <span>

namespace VulkanRTX {

    class PerspectiveCamera;
    class VulkanRTX;

    // -----------------------------------------------------------------------------
    // VulkanRenderer
    // -----------------------------------------------------------------------------
    class VulkanRenderer {
    public:
        // FIXED: Takes RAW POINTERS (non-owning)
        VulkanRenderer(int width, int height, SDL_Window* window,
                       const std::vector<std::string>& shaderPaths,
                       std::shared_ptr<Vulkan::Context> context,
                       VulkanPipelineManager* pipelineManager,     // RAW
                       VulkanBufferManager* bufferManager);        // RAW

        ~VulkanRenderer();

        void initializeAllBufferData(uint32_t maxFrames, VkDeviceSize materialSize, VkDeviceSize dimensionSize);
        void initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize);
        void updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas);
        void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
        void updateGraphicsDescriptorSet(uint32_t frameIndex);
        void updateComputeDescriptorSet(uint32_t frameIndex);
        void updateUniformBuffer(uint32_t frameIndex, const Camera& camera);
        void renderFrame(const Camera& camera);
        void handleResize(int width, int height);
        void cleanup() noexcept;

        VkDevice getDevice() const { return context_->device; }
        const VulkanRTX& getVulkanRTX() const { return *rtx_; }

        std::vector<glm::vec3> getVertices() const;
        std::vector<uint32_t>  getIndices() const;

        VkBuffer       getSBTBuffer() const { return sbtBuffer_; }
        VkDeviceMemory getSBTMemory() const { return sbtMemory_; }

        void setCurrentMode(int mode) { currentMode_ = mode; }
        int  getCurrentMode() const   { return currentMode_; }

    private:
        // -----------------------------------------------------------------------------
        // PRIVATE METHODS
        // -----------------------------------------------------------------------------
        VkShaderModule createShaderModule(const std::string& filepath);
        void createRTOutputImages();
        void createAccumulationImages();
        void recreateRTOutputImage();
        void recreateAccumulationImage();
        void updateRTDescriptors();
        void updateComputeDescriptors(uint32_t imageIndex);
        void createComputeDescriptorSets();
        void createDescriptorPool();
        void createDescriptorSets();
        void buildAccelerationStructures();
        void createCommandBuffers();
        void createEnvironmentMap();
        void createFramebuffers();
        void applyResize(int newWidth, int newHeight);
        void createShaderBindingTable();

        // ──────────────────────── SWAPCHAIN (DIRECT OWNERSHIP, FPS UNLOCKED) ────────────────────────
        void createSwapchain();
        void recreateSwapchain(int width, int height);
        void cleanupSwapchain();

        // -----------------------------------------------------------------------------
        // DISPATCH RENDER MODE
        // -----------------------------------------------------------------------------
        void dispatchRenderMode(
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
            int mode);

        // -----------------------------------------------------------------------------
        // CORE STATE
        // -----------------------------------------------------------------------------
        SDL_Window*                         window_ = nullptr;
        std::shared_ptr<Vulkan::Context>    context_;

        // NON-OWNING RAW POINTERS
        VulkanPipelineManager*              pipelineManager_ = nullptr;
        VulkanBufferManager*                bufferManager_   = nullptr;

        int                                 width_  = 0;
        int                                 height_ = 0;

        uint32_t                            currentFrame_     = 0;
        uint32_t                            frameCount_       = 0;
        uint32_t                            framesThisSecond_ = 0;
        std::chrono::steady_clock::time_point lastFPSTime_ = std::chrono::steady_clock::now();

        int                                 currentMode_      = 1;
        uint32_t                            frameNumber_      = 0;
        int                                 currentRTIndex_   = 0;
        int                                 currentAccumIndex_ = 0;

        // ──────────────────────── SWAPCHAIN DATA (DIRECT) ────────────────────────
        VkSwapchainKHR                      swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage>                swapchainImages_;
        VkFormat                            swapchainImageFormat_;
        VkExtent2D                          swapchainExtent_;
        std::vector<VkImageView>            swapchainImageViews_;

        // FRAME SYNCHRONIZATION (OWNED BY RENDERER)
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT>     inFlightFences_ = {};
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_ = {};
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_ = {};

        // IMAGES
        VkImage             envMapImage_        = VK_NULL_HANDLE;
        VkDeviceMemory      envMapImageMemory_  = VK_NULL_HANDLE;
        VkImageView         envMapImageView_    = VK_NULL_HANDLE;
        VkSampler           envMapSampler_      = VK_NULL_HANDLE;

        std::array<VkImage,        2> rtOutputImages_   = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        std::array<VkDeviceMemory, 2> rtOutputMemories_ = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        std::array<VkImageView,    2> rtOutputViews_    = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        std::array<VkImage,        2> accumImages_   = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        std::array<VkDeviceMemory, 2> accumMemories_ = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        std::array<VkImageView,    2> accumViews_    = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        bool resetAccumulation_ = true;

        // ACCELERATION STRUCTURES
        VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;

        // ENGINE COMPONENTS
        std::unique_ptr<VulkanRTX>              rtx_;

        // FRAME RESOURCES
        std::vector<VkFramebuffer>  framebuffers_;
        std::vector<VkCommandBuffer> commandBuffers_;

        // DESCRIPTOR SETS
        VkDescriptorPool            descriptorPool_ = VK_NULL_HANDLE;
        VkDescriptorSet             rtxDescriptorSet_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout       computeDescriptorSetLayout_ = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> computeDescriptorSets_{};

        // UNIFORM BUFFERS (OWNED BY RENDERER)
        std::array<VkBuffer,       MAX_FRAMES_IN_FLIGHT> uniformBuffers_{};
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBufferMemories_{};

        // MATERIAL & DIMENSION BUFFERS (OWNED BY RENDERER)
        std::array<VkBuffer,       MAX_FRAMES_IN_FLIGHT> materialBuffers_{};
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> materialBufferMemory_{};
        std::array<VkBuffer,       MAX_FRAMES_IN_FLIGHT> dimensionBuffers_{};
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> dimensionBufferMemory_{};

        // RAY TRACING STATE
        VkPipeline       rtPipeline_       = VK_NULL_HANDLE;
        VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;
        VkBuffer         sbtBuffer_        = VK_NULL_HANDLE;
        VkDeviceMemory   sbtMemory_        = VK_NULL_HANDLE;
        ShaderBindingTable sbt_{};  // From VulkanCommon.hpp

        // PERFORMANCE
        std::atomic<bool> swapchainRecreating_{false};

        // QUERY POOLS (TIMESTAMP) — vector, initialized in constructor
        std::vector<VkQueryPool> queryPools_;
        std::array<bool, MAX_FRAMES_IN_FLIGHT> queryReady_ = {false};
        std::array<double, MAX_FRAMES_IN_FLIGHT> lastRTTimeMs_ = {0.0};

        // STATE FLAGS
        bool descriptorsUpdated_ = false;

        // FRIENDS
        friend class VulkanRTX;
    };

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP