#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "engine/core.hpp" // For AMOURANTH, DimensionalNavigator, glm::vec3
#include "engine/Vulkan_init.hpp" // For VulkanRenderer
#include "engine/Vulkan/VulkanPipelineManager.hpp" // For ray-tracing pipeline
#include "engine/SDL3_init.hpp" // For SDL3Initializer
#include "engine/logging.hpp" // For Logging::Logger
#include <vector>
#include <memory>

// Forward declaration
class HandleInput;

class Application {
public:
    Application(const char* title, int width, int height)
        : title_(title), width_(width), height_(height), mode_(1),
          sdl_(std::make_unique<SDL3Initializer>(title, width, height)),
          renderer_(std::make_unique<VulkanRenderer>(
              sdl_->getInstance(), sdl_->getSurface(),
              vertices_, indices_, VK_NULL_HANDLE, VK_NULL_HANDLE, width, height)),
          pipelineManager_(std::make_unique<VulkanRTX::VulkanPipelineManager>(
              renderer_->getContext(), width, height)),
          navigator_(std::make_unique<DimensionalNavigator>(title, width, height, logger_)),
          amouranth_(navigator_.get(), logger_, renderer_->getContext().device,
                     VK_NULL_HANDLE, VK_NULL_HANDLE)
    {
        initialize();
        initializeRayTracing();
        initializeInput();
    }

    void initialize() {
        // Initialize geometry for rasterization (optional, kept for hybrid rendering)
        vertices_ = {
            glm::vec3(-0.5f, -0.5f, 0.0f),
            glm::vec3(0.5f, -0.5f, 0.0f),
            glm::vec3(0.0f, 0.5f, 0.0f)
        };
        indices_ = {0, 1, 2};

        // Initialize rasterization shaders (optional, for hybrid rendering)
        VkShaderModule vertShaderModule = renderer_->createShaderModule("assets/shaders/rasterization/vertex.spv");
        VkShaderModule fragShaderModule = renderer_->createShaderModule("assets/shaders/rasterization/fragment.spv");
        renderer_->setShaderModules(vertShaderModule, fragShaderModule);
        vkDestroyShaderModule(renderer_->getContext().device, vertShaderModule, nullptr);
        vkDestroyShaderModule(renderer_->getContext().device, fragShaderModule, nullptr);
    }

    void initializeRayTracing() {
        // Create acceleration structures (TLAS/BLAS)
        createAccelerationStructures();

        // Update AMOURANTH with ray-tracing resources
        amouranth_.setRayTracingResources(
            pipelineManager_->getRayTracingPipeline(),
            pipelineManager_->getRayTracingPipelineLayout(),
            pipelineManager_->getShaderBindingTable(),
            pipelineManager_->getRayTracingDescriptorSet(),
            topLevelAS_
        );

        // Create storage image for ray-tracing output
        createStorageImage();
    }

    void createAccelerationStructures() {
        // Simplified example: Create a single BLAS for the triangle geometry
        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry.triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData.deviceAddress = renderer_->getVertexBufferDeviceAddress(),
                .vertexStride = sizeof(glm::vec3),
                .maxVertex = static_cast<uint32_t>(vertices_.size()),
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData.deviceAddress = renderer_->getIndexBufferDeviceAddress(),
                .transformData.deviceAddress = 0
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries = &geometry
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
        };
        vkGetAccelerationStructureBuildSizesKHR(
            renderer_->getContext().device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            nullptr,
            &sizeInfo
        );

        // Create BLAS buffer
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeInfo.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VkBuffer blasBuffer;
        vkCreateBuffer(renderer_->getContext().device, &bufferInfo, nullptr, &blasBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(renderer_->getContext().device, blasBuffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                renderer_->getContext().physicalDevice,
                memRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VkDeviceMemory blasMemory;
        vkAllocateMemory(renderer_->getContext().device, &allocInfo, nullptr, &blasMemory);
        vkBindBufferMemory(renderer_->getContext().device, blasBuffer, blasMemory, 0);

        VkAccelerationStructureCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = blasBuffer,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        vkCreateAccelerationStructureKHR(renderer_->getContext().device, &createInfo, nullptr, &blas_);

        // Build BLAS
        VkCommandBuffer commandBuffer = renderer_->beginSingleTimeCommands();
        VkAccelerationStructureBuildGeometryInfoKHR buildInfoBuild = buildInfo;
        buildInfoBuild.dstAccelerationStructure = blas_;
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {
            .primitiveCount = static_cast<uint32_t>(indices_.size() / 3),
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
        VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = {&rangeInfo};
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfoBuild, rangeInfos);
        renderer_->endSingleTimeCommands(commandBuffer);

        // Create TLAS
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };
        VkAccelerationStructureInstanceKHR instance = {
            .transform = transformMatrix,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getAccelerationStructureDeviceAddress(blas_)
        };

        VkBuffer instanceBuffer;
        VkDeviceMemory instanceMemory;
        createBufferWithMemory(instanceBuffer, instanceMemory, sizeof(instance), &instance);
        VkAccelerationStructureGeometryKHR tlasGeometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry.instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data.deviceAddress = getBufferDeviceAddress(instanceBuffer)
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &tlasGeometry;
        vkGetAccelerationStructureBuildSizesKHR(
            renderer_->getContext().device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            nullptr,
            &sizeInfo
        );

        bufferInfo.size = sizeInfo.accelerationStructureSize;
        vkCreateBuffer(renderer_->getContext().device, &bufferInfo, nullptr, &tlasBuffer_);
        vkGetBufferMemoryRequirements(renderer_->getContext().device, tlasBuffer_, &memRequirements);
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanInitializer::findMemoryType(
            renderer_->getContext().physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(renderer_->getContext().device, &allocInfo, nullptr, &tlasMemory_);
        vkBindBufferMemory(renderer_->getContext().device, tlasBuffer_, tlasMemory_, 0);

        createInfo.buffer = tlasBuffer_;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        vkCreateAccelerationStructureKHR(renderer_->getContext().device, &createInfo, nullptr, &topLevelAS_);

        buildInfoBuild = buildInfo;
        buildInfoBuild.dstAccelerationStructure = topLevelAS_;
        rangeInfo.primitiveCount = 1;
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfoBuild, rangeInfos);
        renderer_->endSingleTimeCommands(commandBuffer);

        renderer_->getContext().resourceManager.addAccelerationStructure(blas_);
        renderer_->getContext().resourceManager.addAccelerationStructure(topLevelAS_);
        renderer_->getContext().resourceManager.addBuffer(blasBuffer_);
        renderer_->getContext().resourceManager.addBuffer(tlasBuffer_);
        renderer_->getContext().resourceManager.addMemory(blasMemory_);
        renderer_->getContext().resourceManager.addMemory(tlasMemory_);
        renderer_->getContext().resourceManager.addBuffer(instanceBuffer);
        renderer_->getContext().resourceManager.addMemory(instanceMemory);
    }

    void createStorageImage() {
        VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        vkCreateImage(renderer_->getContext().device, &imageInfo, nullptr, &storageImage_);

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(renderer_->getContext().device, storageImage_, &memRequirements);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                renderer_->getContext().physicalDevice,
                memRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        vkAllocateMemory(renderer_->getContext().device, &allocInfo, nullptr, &storageImageMemory_);
        vkBindImageMemory(renderer_->getContext().device, storageImage_, storageImageMemory_, 0);

        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = storageImage_,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCreateImageView(renderer_->getContext().device, &viewInfo, nullptr, &storageImageView_);

        renderer_->getContext().resourceManager.addImage(storageImage_);
        renderer_->getContext().resourceManager.addImageView(storageImageView_);
        renderer_->getContext().resourceManager.addMemory(storageImageMemory_);

        // Update descriptor set with storage image
        VkDescriptorImageInfo imageInfoDescriptor = {
            .imageView = storageImageView_,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = pipelineManager_->getRayTracingDescriptorSet(),
            .dstBinding = 1, // Matches binding 1 in raygen.rgen
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfoDescriptor
        };
        vkUpdateDescriptorSets(renderer_->getContext().device, 1, &write, 0, nullptr);
    }

    void initializeInput() {
        inputHandler_ = std::make_unique<HandleInput>(&amouranth_, navigator_.get(), logger_);
    }

    void run() {
        while (!sdl_->shouldQuit()) {
            sdl_->pollEvents();
            inputHandler_->handleInput(*this);
            render();
        }
    }

    void render() {
        renderer_->beginFrame();

        // Transition storage image to GENERAL layout for ray tracing
        VkCommandBuffer commandBuffer = renderer_->getCommandBuffer();
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = storageImage_,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCmdPipelineBarrier(commandBuffer,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Record ray-tracing commands
        pipelineManager_->recordRayTracingCommands(
            commandBuffer,
            storageImage_,
            pipelineManager_->getRayTracingDescriptorSet(),
            width_,
            height_
        );

        // Render to swapchain (using graphics pipeline to display ray-traced image)
        amouranth_.render(
            renderer_->getCurrentImageIndex(),
            renderer_->getVertexBuffer(),
            commandBuffer,
            renderer_->getIndexBuffer(),
            pipelineManager_->getGraphicsPipelineLayout(),
            pipelineManager_->getGraphicsDescriptorSet()
        );

        renderer_->endFrame();
    }

    void setRenderMode(int mode) {
        mode_ = mode;
        amouranth_.setMode(mode);
    }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) {
        VkBufferDeviceAddressInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer
        };
        return vkGetBufferDeviceAddress(renderer_->getContext().device, &info);
    }

    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
        VkAccelerationStructureDeviceAddressInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return vkGetAccelerationStructureDeviceAddressKHR(renderer_->getContext().device, &info);
    }

    void createBufferWithMemory(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, void* data) {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        vkCreateBuffer(renderer_->getContext().device, &bufferInfo, nullptr, &buffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(renderer_->getContext().device, buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                renderer_->getContext().physicalDevice,
                memRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        vkAllocateMemory(renderer_->getContext().device, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(renderer_->getContext().device, buffer, memory, 0);

        void* mapped;
        vkMapMemory(renderer_->getContext().device, memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(renderer_->getContext().device, memory);
    }

    std::string title_;
    int width_, height_;
    int mode_;
    std::unique_ptr<SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRenderer> renderer_;
    std::unique_ptr<VulkanRTX::VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<DimensionalNavigator> navigator_;
    AMOURANTH amouranth_;
    Logging::Logger logger_;
    std::unique_ptr<HandleInput> inputHandler_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevelAS_ = VK_NULL_HANDLE;
    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    VkImage storageImage_ = VK_NULL_HANDLE;
    VkImageView storageImageView_ = VK_NULL_HANDLE;
    VkDeviceMemory storageImageMemory_ = VK_NULL_HANDLE;
};

#endif // APPLICATION_HPP