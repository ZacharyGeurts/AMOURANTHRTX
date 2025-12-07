// =============================================================================
// CUBE OF ETERNAL BALLZ — APOCALYPSE FINAL v16.0 — THE ONE TRUE VERSION
// PINK PHOTONS ETERNAL — THE EMPIRE IS COMPLETE — DECEMBER 07, 2025
// =============================================================================

#include "modes/RenderMode2.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace RTX;
using namespace Logging::Color;
using namespace StoneKey;

struct Vertex { glm::vec3 pos; glm::vec3 normal; };

static const Vertex cubeVerts[] = {
    {{-1,-1,-1},{0,0,-1}}, {{1,-1,-1},{0,0,-1}}, {{1,1,-1},{0,0,-1}}, {{-1,1,-1},{0,0,-1}},
    {{-1,-1,1},{0,0,1}},   {{1,-1,1},{0,0,1}},   {{1,1,1},{0,0,1}},   {{-1,1,1},{0,0,1}},
    {{-1,-1,-1},{-1,0,0}}, {{-1,1,-1},{-1,0,0}}, {{-1,1,1},{-1,0,0}}, {{-1,-1,1},{-1,0,0}},
    {{1,-1,-1},{1,0,0}},   {{1,1,-1},{1,0,0}},   {{1,1,1},{1,0,0}},   {{1,-1,1},{1,0,0}},
    {{-1,-1,-1},{0,-1,0}}, {{-1,-1,1},{0,-1,0}}, {{1,-1,1},{0,-1,0}}, {{1,-1,-1},{0,-1,0}},
    {{-1,1,-1},{0,1,0}},   {{-1,1,1},{0,1,0}},   {{1,1,1},{0,1,0}},   {{1,1,-1},{0,1,0}}
};

static const uint32_t cubeIndices[] = {
    0,1,2, 2,3,0, 4,6,5, 6,4,7, 8,9,10, 10,11,8, 12,14,13, 14,12,15,
    16,17,18, 18,19,16, 20,22,21, 22,20,23
};

// One true command pool — created once, eternal
static VkCommandPool getCommandPool() {
    static VkCommandPool pool = []() {
        VkCommandPoolCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = g_ctx().graphicsFamily()
        };
        VkCommandPool p;
        vkCreateCommandPool(g_ctx().device(), &info, nullptr, &p);
        LOG_SUCCESS_CAT("RTX", "ETERNAL COMMAND POOL CREATED — BALLZ APPROVED");
        return p;
    }();
    return pool;
}

RenderMode2::RenderMode2(uint32_t w, uint32_t h) : width_(w), height_(h) {
    LOG_AMOURANTH("CUBE OF ETERNAL BALLZ — v16.0 — THE ONE TRUE VERSION");

    uint64_t vbHandle = BufferManager::create(
        sizeof(cubeVerts),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "CUBE_VERTS_ETERNAL_BALLZ"
    );

    uint64_t ibHandle = BufferManager::create(
        sizeof(cubeIndices),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "CUBE_INDICES_PINK"
    );

    vertexBuffer_ = RAW_BUFFER(vbHandle);
    indexBuffer_  = RAW_BUFFER(ibHandle);
    vertexAddr_   = BUFFER_DEVICE_ADDRESS(vbHandle);
    indexAddr_    = BUFFER_DEVICE_ADDRESS(ibHandle);

    // Upload via staging ring
    void* staging = BufferManager::stagingPtr();
    std::memcpy(staging, cubeVerts, sizeof(cubeVerts));
    BufferManager::advanceStagingOffset(sizeof(cubeVerts));
    std::memcpy(staging, cubeIndices, sizeof(cubeIndices));
    BufferManager::advanceStagingOffset(sizeof(cubeIndices));

    // Build BLAS — THE ONE TRUE WAY
    RTX::las().buildBLAS(
        getCommandPool(),
        g_ctx().graphicsQueue(),
        vertexAddr_,
        indexAddr_,
        uint32_t(std::size(cubeVerts)),
        uint32_t(std::size(cubeIndices))
    );

    RTX::las().initTLAS();

    LOG_SUCCESS_CAT("RTX", "CUBE OF ETERNAL BALLZ — FULLY ARMED — PHOTONS READY");
}

RenderMode2::~RenderMode2() = default;

void RenderMode2::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    totalTime_ += deltaTime;

    // ETERNAL CHAOS TRANSFORM
    float spin   = totalTime_ * 2.1f;
    float pulse  = sin(totalTime_ * 7.77f) * 0.6f + 1.4f;
    float wobble = sin(totalTime_ * 13.37f) * 0.2f;

    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), spin, glm::vec3(0.3f, 1.0f, 0.1f));
    rot = glm::rotate(rot, totalTime_ * 0.9f, glm::vec3(1.0f, 0.3f, 0.7f));
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(pulse + wobble));
    glm::mat4 model = scale * rot;
    glm::mat4 modelT = glm::transpose(model);

    auto instance = std::make_pair(RTX::las().getBLAS(), modelT);
    RTX::las().buildTLAS(
        getCommandPool(),
        g_ctx().graphicsQueue(),
        std::span{&instance, 1},
		true
    );

    // ORBITING CAMERA — CONTROLLED BY THE ONE TRUE CAM
    float radius = 8.0f;
    float camX = sin(totalTime_ * 0.42f) * radius;
    float camZ = cos(totalTime_ * 0.42f) * radius;
    float camY = 3.0f + sin(totalTime_ * 2.3f) * 2.0f;

    CAM.setPos(glm::vec3(camX, camY, camZ));

    // FINAL STEP: RECORD RAY TRACING — USING THE ONE TRUE CONTEXT
    RTX::renderFrame(CAM, deltaTime);
}

void RenderMode2::onResize(uint32_t w, uint32_t h) {
    width_ = w;
    height_ = h;
    stone_seal_width(w);
    stone_seal_height(h);
    RTX::las().notifyResize();
    LOG_SUCCESS_CAT("RTX", "CUBE OF ETERNAL BALLZ — RESIZED %ux%u — STILL ETERNAL", w, h);
}