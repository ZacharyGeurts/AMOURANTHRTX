#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "engine/core.hpp" // For AMOURANTH, DimensionalNavigator, glm::vec3
#include "engine/Vulkan_init.hpp" // For VulkanRenderer
#include "engine/SDL3_init.hpp" // For SDL3Initializer
#include "engine/logging.hpp" // For Logging::Logger
#include <vector>
#include <memory>

// Forward declaration to avoid circular dependency
class HandleInput;

class Application {
public:
    Application(const char* title, int width, int height)
        : title_(title), width_(width), height_(height), mode_(1),
          sdl_(std::make_unique<SDL3Initializer>(title, width, height)),
          renderer_(std::make_unique<VulkanRenderer>(
              sdl_->getInstance(), sdl_->getSurface(),
              vertices_, indices_, VK_NULL_HANDLE, VK_NULL_HANDLE, width, height)),
          navigator_(std::make_unique<DimensionalNavigator>(title, width, height, logger_)),
          amouranth_(navigator_.get(), logger_, renderer_->getContext().device,
                     VK_NULL_HANDLE, VK_NULL_HANDLE)
    {
        initialize();
        initializeInput();
    }

    void initialize() {
        vertices_ = {
            glm::vec3(-0.5f, -0.5f, 0.0f),
            glm::vec3(0.5f, -0.5f, 0.0f),
            glm::vec3(0.0f, 0.5f, 0.0f)
        };
        indices_ = {0, 1, 2};

        VkShaderModule vertShaderModule = renderer_->createShaderModule("shaders/vertex.spv");
        VkShaderModule fragShaderModule = renderer_->createShaderModule("shaders/fragment.spv");

        renderer_->setShaderModules(vertShaderModule, fragShaderModule);

        vkDestroyShaderModule(renderer_->getContext().device, vertShaderModule, nullptr);
        vkDestroyShaderModule(renderer_->getContext().device, fragShaderModule, nullptr);
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
        amouranth_.render(
            renderer_->getCurrentImageIndex(),
            renderer_->getVertexBuffer(),
            renderer_->getCommandBuffer(),
            renderer_->getIndexBuffer(),
            renderer_->getPipelineLayout(),
            renderer_->getDescriptorSet()
        );
        renderer_->endFrame();
    }

    void setRenderMode(int mode) { mode_ = mode; amouranth_.setMode(mode); }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    std::string title_;
    int width_, height_;
    int mode_;
    std::unique_ptr<SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRenderer> renderer_;
    std::unique_ptr<DimensionalNavigator> navigator_;
    AMOURANTH amouranth_;
    Logging::Logger logger_;
    std::unique_ptr<HandleInput> inputHandler_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
};

#endif // APPLICATION_HPP