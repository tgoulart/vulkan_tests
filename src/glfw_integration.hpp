
#pragma once

#include <utility>
#include <vector>

namespace glfw {
    void initialize();
    void shutdown();

    // vulkan integration
    void* createSurface(void *vulkanInstance);
    std::pair<uint32_t, uint32_t> windowSize();
    std::vector<const char *> requiredVulkanExtensions();

    // internal functionality
    bool shouldCloseWindow();
    void pollEvents();
}
