
#pragma once

#include <vector>

namespace glfw {
    void initialize();
    void shutdown();

    void* createSurface(void *vulkanInstance);
    std::vector<const char *> requiredVulkanExtensions();

    bool shouldCloseWindow();
    void pollEvents();
}
