#include "glfw_integration.hpp"

#include <cassert>

#include "include_vulkan.hpp"
#include <GLFW/glfw3.h>

namespace {
    GLFWwindow *_window;
}

namespace glfw {
    void initialize() {
        glfwInit();
        assert(glfwVulkanSupported() == GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        _window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);
    }

    void* createSurface(void *vulkanInstance) {
        VkInstance instance = (VkInstance)vulkanInstance;
        VkSurfaceKHR surface;
        VkResult result = glfwCreateWindowSurface(instance, _window, nullptr, &surface);
        assert(result == VK_SUCCESS);
        return surface;
    }

    void shutdown() {
        glfwDestroyWindow(_window);
        glfwTerminate();
    }

    std::vector<const char *> requiredVulkanExtensions() {
        uint32_t extensionCount = 0;
        const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        // glfw internal pointers are guaranteed to be valid until the library is terminated
        std::vector<const char *> ret;
        for (int i = 0; i < extensionCount; ++i) {
            ret.push_back(extensions[i]);
        }
        return ret;
    }

    bool shouldCloseWindow() {
        return glfwWindowShouldClose(_window);
    }

    void pollEvents() {
        glfwPollEvents();
    }
}
