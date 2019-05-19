
#include "glfw_integration.hpp"
#include "vulkan_integration.hpp"

// https://vulkan-tutorial.com/

int main(int argc, const char * argv[]) {
    vulkan::prepareEnvironment();

    glfw::initialize();
    vulkan::initialize();

    while (!glfw::shouldCloseWindow()) {
        glfw::pollEvents();
    }

    vulkan::shutdown();
    glfw::shutdown();

    return 0;
}
