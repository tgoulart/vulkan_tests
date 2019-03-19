#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vulkan/vulkan.hpp>

#ifndef VK_ICD_FILENAMES
    #error Must define VK_ICD_FILENAMES at build time
#endif

#ifndef VK_LAYER_PATH
    #error Must define VK_LAYER_PATH at build time
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

int main(int argc, const char * argv[]) {
    setenv("VK_ICD_FILENAMES", VK_ICD_FILENAMES, 1);
    setenv("VK_LAYER_PATH", VK_LAYER_PATH, 1);
    setenv("VK_LOADER_DEBUG", "all", 1);

    { // list available layers
        uint32_t instance_layer_count;
        VkResult result = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
        std::cout << instance_layer_count << " layers found:\n";
        if (instance_layer_count > 0) {
            std::unique_ptr<VkLayerProperties[]> instance_layers = std::make_unique<VkLayerProperties[]>(instance_layer_count);
            result = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers.get());
            for (int i = 0; i < instance_layer_count; ++i) {
                std::cout << "-> "<< instance_layers[i].layerName << std::endl;
            }
        }
    }
    std::cout << std::endl;

    { // list available extensions
        uint32_t instance_extension_count;
        VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
        std::cout << instance_extension_count << " extensions found:\n";
        if (instance_extension_count > 0) {
            std::unique_ptr<VkExtensionProperties[]> instance_extensions = std::make_unique<VkExtensionProperties[]>(instance_extension_count);
            result = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.get());
            for (int i = 0; i < instance_extension_count; ++i) {
                std::cout << "-> "<< instance_extensions[i].extensionName << std::endl;
            }
        }
    }
    std::cout << std::endl;

    VkInstanceCreateInfo info = {};

    const char * layerNames[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };
    info.enabledLayerCount = 1;
    info.ppEnabledLayerNames = layerNames;

    const char * extensionNames[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensionNames;

    VkInstance instance;
    VkResult result = vkCreateInstance(&info, NULL, &instance);
    std::cout << "vkCreateInstance result: " << result  << std::endl;
    std::cout << std::endl;

    VkDebugUtilsMessengerCreateInfoEXT createInfo {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        {},
        {},
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        debugCallback,
        nullptr,
    };
    VkDebugUtilsMessengerEXT debugMessenger;
    assert(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) == VK_SUCCESS);

    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    vkDestroyInstance(instance, nullptr);
    return 0;
}
