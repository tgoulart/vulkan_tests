#include "vulkan_integration.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

#include "glfw_integration.hpp"
#include "include_vulkan.hpp"

#ifndef VK_ICD_FILENAMES
    #error Must define VK_ICD_FILENAMES at build time
#endif

#ifndef VK_LAYER_PATH
    #error Must define VK_LAYER_PATH at build time
#endif

namespace {
    VkInstance _instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR _surface = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;
    VkQueue _graphicsQueue = VK_NULL_HANDLE;
}

namespace debug_utils {
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        static auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        static auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void *pUserData) {
        std::cerr << "[validation layer callback] " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
}

namespace config {
    std::vector<const char *> requiredLayers() {
        return { "VK_LAYER_KHRONOS_validation" };
    }

    std::vector<const char*> requiredDeviceExtensions() {
        return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    };

    std::vector<const char *> requiredExtensions() {
        std::vector <const char *> allExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
        std::vector<const char *> glfwExtensions = glfw::requiredVulkanExtensions();
        for (auto candidate : glfwExtensions) {
            for (auto ptr : allExtensions) {
                if (strcmp(candidate, ptr) == 0) {
                    // extension already selected; skip
                    goto SKIP;
                }
            }
            allExtensions.push_back(candidate);
            SKIP:;
        }
        return allExtensions;
    }
}

namespace steps {
    void createInstance() {
        std::vector<const char *> requiredLayers = config::requiredLayers();
        std::vector<const char *> requiredExtensions = config::requiredExtensions();

        { // list available layers
            std::unique_ptr<VkLayerProperties[]> layers = nullptr;
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::cout << layerCount << " layers found:\n";
            if (layerCount > 0) {
                layers = std::make_unique<VkLayerProperties[]>(layerCount);
                vkEnumerateInstanceLayerProperties(&layerCount, layers.get());
                for (int i = 0; i < layerCount; ++i) {
                    std::cout << "-> " << layers[i].layerName << std::endl;
                }
            }
            std::cout << std::endl;

            for (auto layerName : requiredLayers) {
                bool found = false;
                for (int i = 0; i < layerCount; ++i) {
                    if (strcmp(layerName, layers[i].layerName) == 0) {
                        found = true;
                        break;
                    }
                }
                assert(found);
            }
        }

        { // list available extensions
            std::unique_ptr<VkExtensionProperties[]> extensions = nullptr;
            uint32_t extensionCount;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::cout << extensionCount << " extensions found:\n";
            if (extensionCount > 0) {
                extensions = std::make_unique<VkExtensionProperties[]>(extensionCount);
                vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.get());
                for (int i = 0; i < extensionCount; ++i) {
                    std::cout << "-> " << extensions[i].extensionName << std::endl;
                }
            }
            std::cout << std::endl;

            for (auto extensionName : requiredExtensions) {
                bool found = false;
                for (int i = 0; i < extensionCount; ++i) {
                    if (strcmp(extensionName, extensions[i].extensionName) == 0) {
                        found = true;
                        break;
                    }
                }
                assert(found);
            }
        }

        VkInstanceCreateInfo info = {};
        info.enabledLayerCount = requiredLayers.size();
        info.ppEnabledLayerNames = requiredLayers.data();
        info.enabledExtensionCount = requiredExtensions.size();
        info.ppEnabledExtensionNames = requiredExtensions.data();

        VkResult result = vkCreateInstance(&info, NULL, &_instance);
        std::cout << "vkCreateInstance result: " << result << std::endl;
        std::cout << std::endl;
    }

    void setupDebugCallback() {
        VkDebugUtilsMessengerCreateInfoEXT createInfo {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            {},
            {},
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            debug_utils::debugCallback,
            nullptr,
        };

        VkResult result = debug_utils::CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger);
        assert(result == VK_SUCCESS);
    }

    void selectDevice() {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex = UINT32_MAX;

        std::vector<const char *> requiredDeviceExtensions = config::requiredDeviceExtensions();

        { // list physical devices
            auto integratedGPU = std::make_pair<VkPhysicalDevice, uint32_t>(VK_NULL_HANDLE, 0);
            auto discreteGPU = std::make_pair<VkPhysicalDevice, uint32_t>(VK_NULL_HANDLE, 0);

            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
            std::cout << deviceCount << " devices found:\n";
            if (deviceCount > 0) {
                auto devices = std::make_unique<VkPhysicalDevice[]>(deviceCount);
                vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.get());
                for (int i = 0; i < deviceCount; ++i) {
                    VkPhysicalDeviceProperties deviceProperties;
                    vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
                    std::cout << "-> "<< deviceProperties.deviceName << ", type " << deviceProperties.deviceType << std::endl;

                    bool supportsAllExtensions = true;
                    { // list device supported extensions
                        uint32_t extensionCount;
                        vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, nullptr);
                        auto availableExtensions = std::make_unique<VkExtensionProperties[]>(extensionCount);
                        vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, availableExtensions.get());

                        for (auto ext : requiredDeviceExtensions) {
                            bool found = false;
                            for (int i = 0; i < extensionCount; ++i) {
                                if (strcmp(ext, availableExtensions[i].extensionName) == 0) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                supportsAllExtensions = false;
                                break;
                            }
                        }
                    }

                    uint32_t suitableQueueIndex = UINT32_MAX;
                    { // list device queues and check we can use the device
                        uint32_t queueFamilyCount = 0;
                        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, nullptr);
                        // std::cout << queueFamilyCount << " queue families found:\n";
                        if (queueFamilyCount > 0) {
                            auto queueFamilies = std::make_unique<VkQueueFamilyProperties[]>(queueFamilyCount);
                            vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, queueFamilies.get());
                            for (int i = 0; i < queueFamilyCount; ++i) {
                                // std::cout << "-> "<< queueFamilies[i].queueCount << ", " << queueFamilies[i].queueFlags << std::endl;
                                VkBool32 presentSupport = false;
                                vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], i, _surface, &presentSupport);
                                if (queueFamilies[i].queueCount > 0 &&
                                    (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                                    presentSupport) {
                                    suitableQueueIndex = i;
                                    break;
                                }
                            }
                        }
                    }

                    if (supportsAllExtensions && suitableQueueIndex != UINT32_MAX) {
                        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                            integratedGPU = std::make_pair(devices[i], suitableQueueIndex);
                        } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                            discreteGPU = std::make_pair(devices[i], suitableQueueIndex);
                        }
                    }
                }
            }

            if (discreteGPU.first != VK_NULL_HANDLE) {
                physicalDevice = discreteGPU.first;
                queueFamilyIndex = discreteGPU.second;
            } else if (integratedGPU.first != VK_NULL_HANDLE) {
                physicalDevice = integratedGPU.first;
                queueFamilyIndex = integratedGPU.second;
            } else {
                assert(0); // can't find a suitable device
            }

            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
            std::cout << "Using "<< deviceProperties.deviceName << std::endl;

            std::cout << std::endl;
        }

        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures requiredDeviceFeatures = {};
        std::vector<const char *> requiredLayers = config::requiredLayers();

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &requiredDeviceFeatures;
        createInfo.enabledLayerCount = requiredLayers.size();
        createInfo.ppEnabledLayerNames = requiredLayers.data();
        createInfo.enabledExtensionCount = requiredDeviceExtensions.size();
        createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

        VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &_device);
        assert(result == VK_SUCCESS);

        vkGetDeviceQueue(_device, queueFamilyIndex, 0, &_graphicsQueue);
    }

    void setupSurface() {
        _surface = (VkSurfaceKHR)glfw::createSurface(_instance);
    }
}

namespace vulkan {
    void prepareEnvironment() {
        setenv("VK_ICD_FILENAMES", VK_ICD_FILENAMES, 1);
        setenv("VK_LAYER_PATH", VK_LAYER_PATH, 1);
        setenv("VK_LOADER_DEBUG", "all", 1);
    }

    void initialize() {
        steps::createInstance();
        steps::setupDebugCallback();
        steps::setupSurface();
        steps::selectDevice();
    }

    void shutdown() {
        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _surface = VK_NULL_HANDLE;

        debug_utils::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        _debugMessenger = VK_NULL_HANDLE;

        vkDestroyInstance(_instance, nullptr);
        _instance = VK_NULL_HANDLE;
    }
}
