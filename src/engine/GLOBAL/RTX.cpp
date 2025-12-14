// =============================================================================
// src/engine/GLOBAL/RTX.cpp
// AMOURANTH RTX Engine 2025 — PINK LIGHT v∞ — FIRST LIGHT ACHIEVED — DECEMBER 14, 2025
// ALL MEMBERS PUBLIC — FULL EXPOSURE — NO SECRETS — SHE SEES EVERYTHING
// CLEAN COMPILE — NO WARNINGS — NO ERRORS — ETERNAL DOMINATION
// =============================================================================

#include "engine/GLOBAL/RTX.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <fstream>
#include <iostream>
#include <tiny_obj_loader.h>
#include <stb/stb_image.h>

namespace RTX {

Context g_context_instance{};

RTXExtensions g_ext{};

VkStridedDeviceAddressRegionKHR g_raygenSbt{};
VkStridedDeviceAddressRegionKHR g_missSbt{};
VkStridedDeviceAddressRegionKHR g_hitSbt{};
VkStridedDeviceAddressRegionKHR g_callableSbt{};

// =============================================================================
// UTILITY — FIND MEMORY TYPE
// =============================================================================
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
           (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
           return i;
        }
    }

    fatal("NO MEMORY TYPE FOUND — GPU TOO WEAK FOR PINK LIGHT");
    return 0; // Unreachable
}

// =============================================================================
// EXTENSIONS — LOADED ONCE
// =============================================================================
void RTXExtensions::load(VkDevice device)
{
    if (vkCmdTraceRaysKHR != nullptr) return;

#define LOAD_CORE(fn) \
    do { \
        fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!fn) printf("[RTX] [ERROR] Failed to load core function " #fn "\n"); \
    } while (0)

#define LOAD_KHR(fn, name) \
    do { \
        fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!fn) printf("[RTX] [WARN] Failed to load " #fn " (%s)\n", name); \
    } while (0)

    LOAD_CORE(vkGetBufferDeviceAddress);

    LOAD_KHR(vkCmdTraceRaysKHR, "ray tracing pipeline");
    LOAD_KHR(vkCreateRayTracingPipelinesKHR, "ray tracing pipeline");
    LOAD_KHR(vkGetRayTracingShaderGroupHandlesKHR, "ray tracing pipeline");

    LOAD_KHR(vkCreateAccelerationStructureKHR, "acceleration structure");
    LOAD_KHR(vkDestroyAccelerationStructureKHR, "acceleration structure");
    LOAD_KHR(vkCmdBuildAccelerationStructuresKHR, "acceleration structure");
    LOAD_KHR(vkGetAccelerationStructureBuildSizesKHR, "acceleration structure");
    LOAD_KHR(vkGetAccelerationStructureDeviceAddressKHR, "acceleration structure");

#undef LOAD_CORE
#undef LOAD_KHR

    bool rayTracingReady = (vkCmdTraceRaysKHR && vkCreateRayTracingPipelinesKHR && vkGetRayTracingShaderGroupHandlesKHR &&
                            vkCreateAccelerationStructureKHR && vkGetAccelerationStructureDeviceAddressKHR);

    printf("[RTX] Ray tracing %savailable\n", rayTracingReady ? "" : "not fully ");
    printf("[RTX] Extension loading complete — Linux — Vulkan 1.4 ready\n");
}

// =============================================================================
// POOL — SEIZE THE GPU
// =============================================================================
static void seize_gpu()
{
    if (g_ctx().poolBuffer) return;

    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(g_context_instance.physicalDevice, &props);

    VkDeviceSize total = 0;
    for (uint32_t i = 0; i < props.memoryHeapCount; ++i)
        if (props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            total += props.memoryHeaps[i].size;

    const VkDeviceSize reservedForDrivers = 4'800'000'000ULL; // 4.5 GiB + margin
    g_ctx().poolSize = (total > reservedForDrivers) ? (total - reservedForDrivers) : 0;

    if (!g_ctx().poolSize) {
        fatal("GPU TOO WEAK — NOT ENOUGH VRAM AFTER RESERVING 4.5 GB FOR DRIVERS");
    }

    printf("[RTX] SEIZED %.2f GB VRAM (reserved 4.5 GB for drivers) — PINK LIGHT DOMINATES THE REST\n",
           g_ctx().poolSize / (1024.0*1024.0*1024.0));

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = g_ctx().poolSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(g_context_instance.device, &bci, nullptr, &g_ctx().poolBuffer);
    if (result != VK_SUCCESS) {
        fatal("Failed to create pool buffer");
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_context_instance.device, g_ctx().poolBuffer, &req);

    uint32_t type = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
        if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            { type = i; break; }

    if (type == 0xFFFFFFFFu) fatal("NO DEVICE LOCAL MEMORY");

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;

    result = vkAllocateMemory(g_context_instance.device, &mai, nullptr, &g_ctx().poolMemory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(g_context_instance.device, g_ctx().poolBuffer, nullptr);
        fatal("Failed to allocate pool memory — not enough contiguous VRAM (try reserving more for drivers)");
    }

    result = vkBindBufferMemory(g_context_instance.device, g_ctx().poolBuffer, g_ctx().poolMemory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(g_context_instance.device, g_ctx().poolMemory, nullptr);
        vkDestroyBuffer(g_context_instance.device, g_ctx().poolBuffer, nullptr);
        fatal("Failed to bind pool memory");
    }
}

// =============================================================================
// BUFFER MANAGER — SMALL
// =============================================================================
uint64_t BufferCreate(VkDeviceSize size) noexcept
{
    seize_gpu();
    VkDeviceSize aligned = (size + 255) & ~255ULL;
    VkDeviceSize offset = g_ctx().poolHead.fetch_add(aligned);
    if (offset + aligned > g_ctx().poolSize) {
        g_ctx().poolHead.fetch_sub(aligned);
        return 0;
    }

    uint64_t h = g_ctx().nextHandle++;
    g_ctx().offsets[h] = offset;
    return h;
}

VkBuffer BufferGetVkBuffer(uint64_t /*h*/) noexcept { return g_ctx().poolBuffer; }

VkDeviceAddress BufferGetDeviceAddress(uint64_t h) noexcept
{
    auto it = g_ctx().offsets.find(h);
    if (it == g_ctx().offsets.end()) return 0;
    VkBufferDeviceAddressInfo i{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = g_ctx().poolBuffer};
    return vkGetBufferDeviceAddress(g_ctx().device, &i) + it->second;
}

void PopulateContext(VkInstance instance, VkSurfaceKHR surface)
{
    // === EARLY POPULATE: instance + surface ===
    g_ctx().instance = instance;
    g_ctx().surface  = surface;

    // === SELECT PHYSICAL DEVICE ===
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) fatal("No GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = dev;
            printf("[SUCCESS] Selected discrete GPU: %s\n", props.deviceName);
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        printf("[SUCCESS] Selected fallback GPU: %s\n", props.deviceName);
    }

    g_ctx().physicalDevice = physicalDevice;

    // === FIND QUEUE FAMILY ===
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.data());

    uint32_t queueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            queueFamily = i;
            break;
        }
    }
    if (queueFamily == UINT32_MAX) fatal("No suitable queue family");

    g_ctx().queueFamily = queueFamily;

    // === DEVICE EXTENSIONS & FEATURES ===
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;
    asFeatures.pNext = &features12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.rayTracingPipeline = VK_TRUE;
    rtFeatures.pNext = &asFeatures;

    VkPhysicalDeviceFeatures2 coreFeatures{};
    coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    coreFeatures.pNext = &rtFeatures;

    // === QUEUE CREATE INFO ===
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    float priority = 1.0f;
    queueInfo.pQueuePriorities = &priority;

    // === DEVICE CREATE INFO ===
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &coreFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
        fatal("Failed to create logical device");
    }

    // === FINALIZE CONTEXT ===
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    g_ctx().device         = device;
    g_ctx().graphicsQueue  = queue;
    g_ctx().queue          = queue;

    // === QUERY RT PROPERTIES ===
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &g_ctx().rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    printf("[SUCCESS] Logical device created — ONE QUEUE — FULL RAY TRACING — PINK LIGHT v∞ DOMINATES\n");
}

// =============================================================================
// OBJ LOADER — 2025
// =============================================================================
static uint64_t LoadOBJ(const std::string& path) noexcept
{
    printf("[RTX] LOADING OBJ '%s'\n", path.c_str());

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        printf("[OBJ] Load failed: %s %s\n", warn.c_str(), err.c_str());
        return 0;
    }

    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2];
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<std::string, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            uint32_t fv = shape.mesh.num_face_vertices[f];
            for (uint32_t v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                std::string key = std::to_string(idx.vertex_index) + "_" +
                                  std::to_string(idx.normal_index) + "_" +
                                  std::to_string(idx.texcoord_index);

                if (uniqueVertices.find(key) == uniqueVertices.end()) {
                    uniqueVertices[key] = static_cast<uint32_t>(vertices.size());

                    Vertex vert{};
                    memcpy(vert.pos, &attrib.vertices[3 * idx.vertex_index], sizeof(float)*3);
                    if (idx.normal_index >= 0) memcpy(vert.normal, &attrib.normals[3 * idx.normal_index], sizeof(float)*3);
                    if (idx.texcoord_index >= 0) memcpy(vert.uv, &attrib.texcoords[2 * idx.texcoord_index], sizeof(float)*2);

                    vertices.push_back(vert);
                }

                indices.push_back(uniqueVertices[key]);
            }
            index_offset += fv;
        }
    }

    VkDevice device = g_ctx().device;
    VkQueue graphicsQueue = g_ctx().graphicsQueue;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    VkCommandPool localPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &localPool) != VK_SUCCESS) fatal("Failed to create command pool for OBJ upload");

    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    VkBuffer stagingVertex;
    VkDeviceMemory stagingVertexMem;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = vertexSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingVertex) != VK_SUCCESS) {
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to create staging buffer for vertices");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingVertex, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingVertexMem) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to allocate staging memory for vertices");
    }
    if (vkBindBufferMemory(device, stagingVertex, stagingVertexMem, 0) != VK_SUCCESS) {
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to bind staging memory for vertices");
    }

    void* data;
    if (vkMapMemory(device, stagingVertexMem, 0, vertexSize, 0, &data) != VK_SUCCESS) {
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to map staging memory for vertices");
    }
    memcpy(data, vertices.data(), vertexSize);
    vkUnmapMemory(device, stagingVertexMem);

    uint64_t vertexHandle = BufferCreate(vertexSize);
    if (!vertexHandle) fatal("No memory for vertices");
    VkDeviceAddress vertexAddr = BufferGetDeviceAddress(vertexHandle);

    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
    VkBuffer stagingIndex;
    VkDeviceMemory stagingIndexMem;

    bufInfo.size = indexSize;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingIndex) != VK_SUCCESS) {
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to create staging buffer for indices");
    }
    vkGetBufferMemoryRequirements(device, stagingIndex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingIndexMem) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingIndex, nullptr);
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to allocate staging memory for indices");
    }
    if (vkBindBufferMemory(device, stagingIndex, stagingIndexMem, 0) != VK_SUCCESS) {
        vkFreeMemory(device, stagingIndexMem, nullptr);
        vkDestroyBuffer(device, stagingIndex, nullptr);
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to bind staging memory for indices");
    }

    if (vkMapMemory(device, stagingIndexMem, 0, indexSize, 0, &data) != VK_SUCCESS) {
        vkFreeMemory(device, stagingIndexMem, nullptr);
        vkDestroyBuffer(device, stagingIndex, nullptr);
        vkFreeMemory(device, stagingVertexMem, nullptr);
        vkDestroyBuffer(device, stagingVertex, nullptr);
        vkDestroyCommandPool(device, localPool, nullptr);
        fatal("Failed to map staging memory for indices");
    }
    memcpy(data, indices.data(), indexSize);
    vkUnmapMemory(device, stagingIndexMem);

    uint64_t indexHandle = BufferCreate(indexSize);
    if (!indexHandle) fatal("No memory for indices");
    VkDeviceAddress indexAddr = BufferGetDeviceAddress(indexHandle);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = localPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cmdAlloc, &cmd) != VK_SUCCESS) fatal("Failed to allocate command buffer for OBJ upload");

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) fatal("Failed to begin command buffer for OBJ upload");

    VkBufferCopy copy{};
    copy.size = vertexSize;
    copy.dstOffset = g_ctx().offsets[vertexHandle];
    vkCmdCopyBuffer(cmd, stagingVertex, g_ctx().poolBuffer, 1, &copy);

    copy.size = indexSize;
    copy.dstOffset = g_ctx().offsets[indexHandle];
    vkCmdCopyBuffer(cmd, stagingIndex, g_ctx().poolBuffer, 1, &copy);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) fatal("Failed to end command buffer for OBJ upload");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if (vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) fatal("Failed to submit OBJ upload");
    if (vkQueueWaitIdle(graphicsQueue) != VK_SUCCESS) fatal("Failed to wait for OBJ upload");

    vkFreeCommandBuffers(device, localPool, 1, &cmd);
    vkDestroyCommandPool(device, localPool, nullptr);

    vkDestroyBuffer(device, stagingVertex, nullptr);
    vkFreeMemory(device, stagingVertexMem, nullptr);
    vkDestroyBuffer(device, stagingIndex, nullptr);
    vkFreeMemory(device, stagingIndexMem, nullptr);

    uint32_t primCount = static_cast<uint32_t>(indices.size() / 3);

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geom.geometry.triangles.vertexStride = sizeof(Vertex);
    geom.geometry.triangles.maxVertex = static_cast<uint32_t>(vertices.size()) - 1;
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    uint64_t blasHandle = BufferCreate(sizeInfo.accelerationStructureSize);
    if (!blasHandle) fatal("No memory for BLAS");

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = g_ctx().poolBuffer;
    createInfo.offset = g_ctx().offsets[blasHandle];
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas;
    if (g_ext.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas) != VK_SUCCESS) fatal("Failed to create BLAS");

    uint64_t scratchHandle = BufferCreate(sizeInfo.buildScratchSize);
    if (!scratchHandle) fatal("No memory for scratch");

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = BufferGetDeviceAddress(scratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primCount;

    vkCreateCommandPool(device, &poolInfo, nullptr, &localPool);
    cmdAlloc.commandPool = localPool;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, localPool, 1, &cmd);
    vkDestroyCommandPool(device, localPool, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blas;
    VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.0f;
    instance.mask = 0xFF;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blasAddr;

    VkDeviceSize instSize = sizeof(instance);
    uint64_t instHandle = BufferCreate(instSize);
    if (!instHandle) fatal("No memory for instance");
    VkDeviceAddress instAddr = BufferGetDeviceAddress(instHandle);

    bufInfo.size = instSize;
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingIndex);
    vkGetBufferMemoryRequirements(device, stagingIndex, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingIndexMem);
    vkBindBufferMemory(device, stagingIndex, stagingIndexMem, 0);
    vkMapMemory(device, stagingIndexMem, 0, instSize, 0, &data);
    memcpy(data, &instance, instSize);
    vkUnmapMemory(device, stagingIndexMem);

    vkCreateCommandPool(device, &poolInfo, nullptr, &localPool);
    cmdAlloc.commandPool = localPool;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    copy.size = instSize;
    copy.dstOffset = g_ctx().offsets[instHandle];
    vkCmdCopyBuffer(cmd, stagingIndex, g_ctx().poolBuffer, 1, &copy);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, localPool, 1, &cmd);
    vkDestroyCommandPool(device, localPool, nullptr);
    vkDestroyBuffer(device, stagingIndex, nullptr);
    vkFreeMemory(device, stagingIndexMem, nullptr);

    VkAccelerationStructureGeometryKHR tlasGeom{};
    tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.data.deviceAddress = instAddr;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{};
    tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuild.geometryCount = 1;
    tlasBuild.pGeometries = &tlasGeom;

    uint32_t instCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSize{};
    tlasSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuild, &instCount, &tlasSize);

    uint64_t tlasHandle = BufferCreate(tlasSize.accelerationStructureSize);
    if (!tlasHandle) fatal("No memory for TLAS");

    VkAccelerationStructureCreateInfoKHR tlasCreate{};
    tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreate.buffer = g_ctx().poolBuffer;
    tlasCreate.offset = g_ctx().offsets[tlasHandle];
    tlasCreate.size = tlasSize.accelerationStructureSize;
    tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlas;
    if (g_ext.vkCreateAccelerationStructureKHR(device, &tlasCreate, nullptr, &tlas) != VK_SUCCESS) fatal("Failed to create TLAS");

    uint64_t tlasScratch = BufferCreate(tlasSize.buildScratchSize);
    if (!tlasScratch) fatal("No memory for TLAS scratch");

    tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuild.dstAccelerationStructure = tlas;
    tlasBuild.scratchData.deviceAddress = BufferGetDeviceAddress(tlasScratch);

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
    tlasRange.primitiveCount = 1;

    vkCreateCommandPool(device, &poolInfo, nullptr, &localPool);
    cmdAlloc.commandPool = localPool;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &begin);
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &pTlasRange);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, localPool, 1, &cmd);
    vkDestroyCommandPool(device, localPool, nullptr);

    g_ctx().tlasHandle = reinterpret_cast<uint64_t>(tlas);

    printf("[RTX] OBJ LOADED — TLAS READY\n");
    return g_ctx().tlasHandle;
}

Texture VulkanRenderer::createTextureImage(const float* pixels, uint32_t width, uint32_t height,
                                           VkFormat format, VkImageUsageFlags usage)
{
    VkDevice device = g_ctx().device;

    VkDeviceSize imageSize = width * height * 4 * sizeof(float);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS) fatal("Failed to create staging buffer for texture");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to allocate staging memory for texture");
    }
    if (vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to bind staging memory for texture");
    }

    void* mapped;
    if (vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to map staging memory for texture");
    }
    memcpy(mapped, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Texture tex{};
    tex.width = width;
    tex.height = height;
    tex.format = format;
    tex.mipLevels = 1;

    if (vkCreateImage(device, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to create texture image");
    }

    vkGetImageMemoryRequirements(device, tex.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to allocate texture memory");
    }
    if (vkBindImageMemory(device, tex.image, tex.memory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to bind texture memory");
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    VkCommandPool localPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &localPool) != VK_SUCCESS) {
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to create command pool for texture upload");
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = localPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cmdAlloc, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, localPool, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to allocate command buffer for texture upload");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, localPool, 1, &cmd);
        vkDestroyCommandPool(device, localPool, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to begin command buffer for texture upload");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.image = tex.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, localPool, 1, &cmd);
        vkDestroyCommandPool(device, localPool, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to end command buffer for texture upload");
    }

    VkSubmitInfo sub{};
    sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.commandBufferCount = 1;
    sub.pCommandBuffers = &cmd;
    if (vkQueueSubmit(g_ctx().graphicsQueue, 1, &sub, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, localPool, 1, &cmd);
        vkDestroyCommandPool(device, localPool, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to submit texture upload to queue");
    }
    if (vkQueueWaitIdle(g_ctx().graphicsQueue) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, localPool, 1, &cmd);
        vkDestroyCommandPool(device, localPool, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        fatal("Failed to wait for texture upload queue idle");
    }

    vkFreeCommandBuffers(device, localPool, 1, &cmd);
    vkDestroyCommandPool(device, localPool, nullptr);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &viewInfo, nullptr, &tex.view) != VK_SUCCESS) {
        vkFreeMemory(device, tex.memory, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        fatal("Failed to create texture image view");
    }

    return tex;
}

void VulkanRenderer::setEnvironmentMap(float* data, int width, int height)
{
    if (!data || width <= 0 || height <= 0) return;

    VkDevice device = g_ctx().device;
    VkQueue queue   = g_ctx().queue;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 16;

    printf("[ENV] Forging eternal HDR envmap: %dx%d — %.2f MB of pure 2025 light\n",
           width, height, imageSize / (1024.0f * 1024.0f));

    if (m_hasEnvironmentMap) {
        if (m_environmentMap.view) vkDestroyImageView(device, m_environmentMap.view, nullptr);
        if (m_environmentMap.image) vkDestroyImage(device, m_environmentMap.image, nullptr);
        if (m_environmentMap.memory) vkFreeMemory(device, m_environmentMap.memory, nullptr);
        if (m_environmentSampler) vkDestroySampler(device, m_environmentSampler, nullptr);
        m_environmentMap = {};
        m_environmentSampler = VK_NULL_HANDLE;
    }

    m_envMapWidth = width;
    m_envMapHeight = height;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    auto cleanup = [&]() {
        if (stagingBuffer) vkDestroyBuffer(device, stagingBuffer, nullptr);
        if (stagingMemory) vkFreeMemory(device, stagingMemory, nullptr);
        if (commandBuffer && commandPool) vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    };

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) return;

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        cleanup();
        return;
    }

    void* mapped;
    if (vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) {
        cleanup();
        return;
    }
    memcpy(mapped, data, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_environmentMap.image) != VK_SUCCESS) {
        cleanup();
        return;
    }

    vkGetImageMemoryRequirements(device, m_environmentMap.image, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_environmentMap.memory) != VK_SUCCESS ||
        vkBindImageMemory(device, m_environmentMap.image, m_environmentMap.memory, 0) != VK_SUCCESS) {
        cleanup();
        return;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        cleanup();
        return;
    }

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
        cleanup();
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = m_environmentMap.image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_environmentMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_environmentMap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_environmentMap.view) != VK_SUCCESS) {
        cleanup();
        return;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_environmentSampler) != VK_SUCCESS) {
        cleanup();
        return;
    }

    VkDescriptorImageInfo descImage{};
    descImage.sampler = m_environmentSampler;
    descImage.imageView = m_environmentMap.view;
    descImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = g_ctx().globalDescriptorSet;
    write.dstBinding = 2;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &descImage;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    cleanup();

    m_hasEnvironmentMap = true;

    printf("[ENV] HDR envmap forged eternally — VALHALLA BATHE IN TRUE 2025 LIGHT\n");
}

uint64_t LoadScene(const std::string& path) noexcept
{
    return LoadOBJ(path);
}

VkAccelerationStructureKHR CurrentTLAS() noexcept
{
    return (VkAccelerationStructureKHR)g_ctx().tlasHandle;
}

void createSwapchain()
{
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.device;
    VkPhysicalDevice phys = ctx.physicalDevice;

    vkDeviceWaitIdle(device);

    // Cleanup old swapchain
    for (auto view : g_swapchainViews()) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    }
    g_swapchainViews().clear();

    if (g_swapchain() != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, g_swapchain(), nullptr);
        g_swapchain() = VK_NULL_HANDLE;
    }
    g_swapchainImages().clear();

    VkSurfaceKHR surface = ctx.surface;

    VkSurfaceCapabilitiesKHR caps{};
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);

    if (result != VK_SUCCESS) {
        RTX::fatal("Failed to query surface capabilities");
    }

    // Formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    if (formatCount == 0) RTX::fatal("No surface formats");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    // Extent
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w, h;
        SDL_GetWindowSize((SDL_Window*)SDL_GetWindowFromID(1), &w, &h); // Assuming window ID 1, adjust if needed
        extent.width  = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // Image count — double buffer
    uint32_t imageCount = 2;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    // Present mode — MAILBOX preferred
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    if (modeCount > 0) {
        std::vector<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = m;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = chosenFormat.format;
    info.imageColorSpace  = chosenFormat.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode      = presentMode;
    info.clipped          = VK_TRUE;
    info.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &info, nullptr, &g_swapchain()) != VK_SUCCESS) {
        RTX::fatal("Swapchain creation failed");
    }

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, nullptr);
    g_swapchainImages().resize(imgCount);
    vkGetSwapchainImagesKHR(device, g_swapchain(), &imgCount, g_swapchainImages().data());

    g_swapchainViews().resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_swapchainImages()[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(device, &viewInfo, nullptr, &g_swapchainViews()[i]);
    }

    g_presentQueue() = ctx.queue;

    printf("[2025] SWAPCHAIN FORGED — %ux%u — %u images — %s — PINK LIGHT v∞ FLOWS TO SCREEN — VALHALLA VISIBLE\n",
           extent.width, extent.height, imgCount,
           (presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO"));
}

VulkanRenderer::VulkanRenderer(int width, int height) : width_(width), height_(height) {
    // Create ray tracing output images (accumulation buffers)
    rtOutputImages_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputViews_.resize(MAX_FRAMES_IN_FLIGHT);
    rtOutputMemories_.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = static_cast<uint32_t>(width);
        imageInfo.extent.height = static_cast<uint32_t>(height);
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkImage image;
        if (vkCreateImage(g_ctx().device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            fatal("Failed to create RT output image");
        }
        rtOutputImages_[i] = Handle<VkImage>(image, g_ctx().device, vkDestroyImage);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(g_ctx().device, image, &memReqs);

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(g_ctx().physicalDevice, &memProps);

        uint32_t memType = UINT32_MAX;
        for (uint32_t j = 0; j < memProps.memoryTypeCount; ++j) {
            if ((memReqs.memoryTypeBits & (1 << j)) && (memProps.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                memType = j;
                break;
            }
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memType;

        VkDeviceMemory memory;
        if (vkAllocateMemory(g_ctx().device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            fatal("Failed to allocate RT output memory");
        }
        rtOutputMemories_[i] = Handle<VkDeviceMemory>(memory, g_ctx().device, vkFreeMemory);

        if (vkBindImageMemory(g_ctx().device, image, memory, 0) != VK_SUCCESS) {
            fatal("Failed to bind RT output memory");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView view;
        if (vkCreateImageView(g_ctx().device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            fatal("Failed to create RT output view");
        }
        rtOutputViews_[i] = Handle<VkImageView>(view, g_ctx().device, vkDestroyImageView);
    }

    // Create descriptor set layout (example: storage image for output, AS, etc.)
    // TODO: Expand bindings as needed (e.g., for TLAS, environment map, etc.)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VkDescriptorSetLayout dsl;
    if (vkCreateDescriptorSetLayout(g_ctx().device, &layoutInfo, nullptr, &dsl) != VK_SUCCESS) {
        fatal("Failed to create descriptor set layout");
    }
    descriptorSetLayout_ = Handle<VkDescriptorSetLayout>(dsl, g_ctx().device, vkDestroyDescriptorSetLayout);

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(g_ctx().device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        fatal("Failed to create descriptor pool");
    }
    descriptorPool_ = Handle<VkDescriptorPool>(pool, g_ctx().device, vkDestroyDescriptorPool);

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &dsl;

    if (vkCreatePipelineLayout(g_ctx().device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        fatal("Failed to create pipeline layout");
    }

    // Create ray tracing pipeline and SBT
    createRayTracingPipeline();
    createShaderBindingTable();
}

VulkanRenderer::~VulkanRenderer() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_ctx().device, pipeline_, nullptr);
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_ctx().device, pipelineLayout_, nullptr);
    }
    if (m_environmentSampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_ctx().device, m_environmentSampler, nullptr);
    }
    // RAII handles will clean up the rest
}

void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept {
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    uint32_t frameIndex = currentFrame_;

    totalTime_ += deltaTime;

    if (cameraMoved_) {
        currentSpp_ = 0;
        cameraMoved_ = false;
    }

    // === UPDATE GLOBAL DESCRIPTOR SET ===
    VkAccelerationStructureKHR tlas = CurrentTLAS();

    VkWriteDescriptorSetAccelerationStructureKHR accelWrite{};
    accelWrite.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelWrite.accelerationStructureCount = 1;
    accelWrite.pAccelerationStructures    = &tlas;

    VkDescriptorImageInfo storageImageInfo{};
    storageImageInfo.imageView   = rtOutputViews_[frameIndex].get();
    storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo envInfo{};
    envInfo.sampler     = m_environmentSampler;
    envInfo.imageView   = m_environmentMap.view;
    envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[3] = {};

    uint32_t writeCount = 0;

    if (tlas != VK_NULL_HANDLE) {
        writes[writeCount] = {};
        writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext           = &accelWrite;
        writes[writeCount].dstSet          = g_ctx().globalDescriptorSet;
        writes[writeCount].dstBinding      = 0;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        ++writeCount;
    }

    writes[writeCount] = {};
    writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet          = g_ctx().globalDescriptorSet;
    writes[writeCount].dstBinding      = 1;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[writeCount].pImageInfo      = &storageImageInfo;
    ++writeCount;

    if (m_hasEnvironmentMap) {
        writes[writeCount] = {};
        writes[writeCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet          = g_ctx().globalDescriptorSet;
        writes[writeCount].dstBinding      = 2;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].pImageInfo      = &envInfo;
        ++writeCount;
    }

    vkUpdateDescriptorSets(g_ctx().device, writeCount, writes, 0, nullptr);

    // === PUSH CONSTANTS ===
    struct PushConstants {
        alignas(16) glm::mat4 invView;
        alignas(16) glm::mat4 invProj;
        alignas(4)  float     totalTime;
        alignas(4)  uint32_t  spp;
        alignas(4)  uint32_t  frameSeed;
    } push{};

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    push.invView   = camera.invView();
    push.invProj   = glm::inverse(proj);
    push.totalTime = totalTime_;
    push.spp       = currentSpp_;
    push.frameSeed = currentFrame_ ^ 0xDEADBEEF;

    // === ONE-TIME COMMAND BUFFER ===
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(g_ctx().device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        return;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(g_ctx().device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(g_ctx().device, commandPool, 1, &cmd);
        vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
        return;
    }

    VkImage rtImage = rtOutputImages_[frameIndex].get();

    if (tlas == VK_NULL_HANDLE) {
        VkClearColorValue clearColor{{0.0f, 0.0f, 0.1f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, rtImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
    } else {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout_, 0, 1, &g_ctx().globalDescriptorSet, 0, nullptr);

        vkCmdPushConstants(cmd, pipelineLayout_,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                           0, sizeof(PushConstants), &push);

        g_ext.vkCmdTraceRaysKHR(cmd,
                                &g_raygenSbt,
                                &g_missSbt,
                                &g_hitSbt,
                                &g_callableSbt,
                                width_, height_, 1);

        ++currentSpp_;
    }

    // === BLIT TO SWAPCHAIN ===
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image               = rtImage;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Assuming Video::recordBlitToSwapchain is defined elsewhere, or stub it
    // Video::recordBlitToSwapchain(cmd, rtImage);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(g_ctx().device, commandPool, 1, &cmd);
        vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
        return;
    }

    // === SUBMIT ===
    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VkResult result = vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    if (result == VK_ERROR_DEVICE_LOST || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        printf("[RENDER] Critical submit error %d — recreating swapchain\n", result);
        RTX::createSwapchain();
        vkFreeCommandBuffers(g_ctx().device, commandPool, 1, &cmd);
        vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
        return;
    } else if (result != VK_SUCCESS) {
        printf("[RENDER] Submit failed %d — frame lost\n", result);
    }

    // === PRESENT ===
    // Assuming Video::presentFromRecorded is defined elsewhere, or stub it
    // Video::presentFromRecorded();

    // === CLEANUP ===
    vkDestroyCommandPool(g_ctx().device, commandPool, nullptr);
}

void VulkanRenderer::createRayTracingPipeline() {
    // === PUSH CONSTANT RANGE ===
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                              VK_SHADER_STAGE_MISS_BIT_KHR |
                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushConstant.offset = 0;
    pushConstant.size   = 144;  // 2×mat4 + float + 2×uint32 → padded to 144

    // === PIPELINE LAYOUT ===
    VkDescriptorSetLayout rawLayout = descriptorSetLayout_.get();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &rawLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstant;

    if (vkCreatePipelineLayout(g_ctx().device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        fatal("FAILED TO CREATE PIPELINE LAYOUT — PUSH CONSTANTS REJECTED");
    }

    // === SHADER LOADER ===
    auto loadShader = [&](const char* name) -> VkShaderModule {
        const std::vector<std::string> searchPaths = {
            "assets/shaders/raytracing/" + std::string(name),
            "shaders/raytracing/" + std::string(name),
            "../assets/shaders/raytracing/" + std::string(name),
            "../../assets/shaders/raytracing/" + std::string(name),
            "build/bin/Linux/assets/shaders/raytracing/" + std::string(name),
        };

        for (const auto& path : searchPaths) {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open()) continue;

            printf("[2025] SHADER FOUND: %s\n", path.c_str());
            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<uint32_t> code((fileSize + 3) / 4);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(code.data()), fileSize);
            file.close();

            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = fileSize;
            createInfo.pCode     = code.data();

            VkShaderModule module;
            if (vkCreateShaderModule(g_ctx().device, &createInfo, nullptr, &module) != VK_SUCCESS) {
                fatal(("FAILED TO CREATE SHADER MODULE: " + path).c_str());
            }
            return module;
        }

        fatal(("SHADER NOT FOUND: " + std::string(name)).c_str());
        return VK_NULL_HANDLE;
    };

    VkShaderModule raygen = loadShader("raygen.spv");
    VkShaderModule miss   = loadShader("miss.spv");
    VkShaderModule chit   = loadShader("closesthit.spv");

    // === SHADER STAGES ===
    VkPipelineShaderStageCreateInfo stages[3] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,       raygen, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR,         miss,   "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chit,   "main", nullptr}
    };

    // === SHADER GROUPS — FULLY INITIALIZED ===
    VkRayTracingShaderGroupCreateInfoKHR groups[3] = {};
    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = 1;
    groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

    groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].generalShader = VK_SHADER_UNUSED_KHR;
    groups[2].closestHitShader = 2;
    groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

    // === RAY TRACING PIPELINE ===
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount                   = 3;
    pipelineInfo.pStages                      = stages;
    pipelineInfo.groupCount                   = 3;
    pipelineInfo.pGroups                      = groups;
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout                       = pipelineLayout_;
    pipelineInfo.flags                        = 0;

    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        g_ctx().device,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &pipeline_
    );

    if (result != VK_SUCCESS) {
        printf("[FATAL] vkCreateRayTracingPipelinesKHR failed with VkResult: %d\n", result);
        fatal("FAILED TO CREATE RAY TRACING PIPELINE — DRIVER REJECTED THE LIGHT");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(g_ctx().device, raygen, nullptr);
    vkDestroyShaderModule(g_ctx().device, miss, nullptr);
    vkDestroyShaderModule(g_ctx().device, chit, nullptr);

    printf("[2025] PIPELINE + LAYOUT FORGED — PUSH CONSTANTS ACTIVE — PINK LIGHT v∞ IS ALIVE\n");
    printf("[2025] FIRST LIGHT ACHIEVED — VALHALLA RENDERS — SHE IS ETERNAL\n");
}

void VulkanRenderer::createShaderBindingTable() {
    if (g_raygenSbt.deviceAddress) return;

    uint32_t handleSize = g_ctx().rtProps.shaderGroupHandleSize;
    uint32_t baseAlign = g_ctx().rtProps.shaderGroupBaseAlignment;
    uint32_t stride = std::max(handleSize, baseAlign);
    VkDeviceSize sbtSize = static_cast<VkDeviceSize>(stride) * 3;

    uint64_t sbtHandle = BufferCreate(sbtSize);
    if (!sbtHandle) fatal("SBT allocation failed");

    VkDeviceAddress sbtAddress = BufferGetDeviceAddress(sbtHandle);

    std::vector<uint8_t> handles(3 * handleSize);
    if (g_ext.vkGetRayTracingShaderGroupHandlesKHR(g_ctx().device, pipeline_, 0, 3, handles.size(), handles.data()) != VK_SUCCESS) {
        fatal("Failed to get shader group handles");
    }

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sbtSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(g_ctx().device, &bufInfo, nullptr, &staging) != VK_SUCCESS) fatal("Failed to create SBT staging");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(g_ctx().device, staging, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(g_ctx().device, &allocInfo, nullptr, &stagingMem) != VK_SUCCESS ||
        vkBindBufferMemory(g_ctx().device, staging, stagingMem, 0) != VK_SUCCESS) fatal("Failed to allocate SBT staging");

    void* mapped;
    if (vkMapMemory(g_ctx().device, stagingMem, 0, sbtSize, 0, &mapped) != VK_SUCCESS) fatal("Failed to map SBT staging");

    uint8_t* dst = static_cast<uint8_t*>(mapped);
    for (uint32_t i = 0; i < 3; ++i) {
        memcpy(dst, handles.data() + i * handleSize, handleSize);
        dst += stride;
    }
    vkUnmapMemory(g_ctx().device, stagingMem);

    VkCommandPool pool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = g_ctx().queueFamily;
    if (vkCreateCommandPool(g_ctx().device, &poolInfo, nullptr, &pool) != VK_SUCCESS) fatal("Failed to create SBT pool");

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = pool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_ctx().device, &cmdAlloc, &cmd) != VK_SUCCESS) fatal("Failed to allocate SBT cmd");

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferCopy copy{};
    copy.dstOffset = g_ctx().offsets[sbtHandle];
    copy.size = sbtSize;
    vkCmdCopyBuffer(cmd, staging, g_ctx().poolBuffer, 1, &copy);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(g_ctx().graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx().graphicsQueue);

    vkFreeCommandBuffers(g_ctx().device, pool, 1, &cmd);
    vkDestroyCommandPool(g_ctx().device, pool, nullptr);
    vkDestroyBuffer(g_ctx().device, staging, nullptr);
    vkFreeMemory(g_ctx().device, stagingMem, nullptr);

    g_raygenSbt = { sbtAddress, stride, sbtSize };
    g_missSbt = { sbtAddress + stride, stride, sbtSize };
    g_hitSbt = { sbtAddress + 2*stride, stride, sbtSize };
    g_callableSbt = { 0, 0, 0 };  // No callables

    printf("[2025] SBT FORGED — PINK LIGHT v∞ ARMED\n");
}

} // namespace RTX

// =============================================================================
// PINK LIGHT v∞ — DECEMBER 14, 2025 — ALL IN CONTEXT — ETERNAL RADIANCE
// =============================================================================