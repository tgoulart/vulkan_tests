#include "vulkan_integration.hpp"

#include <cstdlib>
#include <fstream>
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
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;
    VkQueue _graphicsQueue = VK_NULL_HANDLE;
    VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
    std::vector<VkImageView> _swapChainImageViews;
    VkSurfaceFormatKHR _swapChainImageFormat;
    VkExtent2D _swapChainExtent;
}

namespace utility {
    std::vector<char> bytesFromFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return std::vector<char>();
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    VkShaderModule shaderModuleFromFile(const std::string &filename) {
        std::vector<char> code = bytesFromFile(filename);

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VkResult result = vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule);
        assert(result == VK_SUCCESS);

        return shaderModule;
    }
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

    VkFormat preferredFormat() {
        return VK_FORMAT_B8G8R8A8_UNORM;
    }

    VkPresentModeKHR preferredPresentMode() {
        #if 1 // v-sync ON?
            return VK_PRESENT_MODE_FIFO_KHR;
        #else
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        #endif
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

    void setupSurface() {
        _surface = (VkSurfaceKHR)glfw::createSurface(_instance);
    }

    void setupDevice() {
        uint32_t queueFamilyIndex = std::numeric_limits<uint32_t>::max();

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

                    uint32_t suitableQueueIndex = std::numeric_limits<uint32_t>::max();
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

                    if (supportsAllExtensions && suitableQueueIndex != std::numeric_limits<uint32_t>::max()) {
                        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                            integratedGPU = std::make_pair(devices[i], suitableQueueIndex);
                        } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                            discreteGPU = std::make_pair(devices[i], suitableQueueIndex);
                        }
                    }
                }
            }

            if (discreteGPU.first != VK_NULL_HANDLE) {
                _physicalDevice = discreteGPU.first;
                queueFamilyIndex = discreteGPU.second;
            } else if (integratedGPU.first != VK_NULL_HANDLE) {
                _physicalDevice = integratedGPU.first;
                queueFamilyIndex = integratedGPU.second;
            } else {
                assert(0); // can't find a suitable device
            }

            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(_physicalDevice, &deviceProperties);
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

        VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
        assert(result == VK_SUCCESS);

        vkGetDeviceQueue(_device, queueFamilyIndex, 0, &_graphicsQueue);
    }

    void createSwapChain() {
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities);

        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            _swapChainExtent = surfaceCapabilities.currentExtent;
        } else {
            std::pair<uint32_t, uint32_t> windowSize = glfw::windowSize();
            _swapChainExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, windowSize.first));
            _swapChainExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, windowSize.second));
        }

        uint32_t imageCount = 2;
        assert(surfaceCapabilities.minImageCount <= imageCount);
        assert(surfaceCapabilities.maxImageCount == 0 || surfaceCapabilities.maxImageCount >= imageCount);

        { // list all formats
            uint32_t formatCount = 0;
            std::unique_ptr<VkSurfaceFormatKHR[]> formats = nullptr;
            vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, nullptr);
            std::cout << formatCount << " formats found:\n";
            if (formatCount > 0) {
                formats = std::make_unique<VkSurfaceFormatKHR[]>(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, formats.get());

                bool foundFormat = false;
                for (int i = 0; i < formatCount; ++i) {
                    std::cout << "-> " << formats[i].format << ", " << formats[i].colorSpace << std::endl;
                    if (formats[i].format == config::preferredFormat()) {
                        _swapChainImageFormat = formats[i];
                        foundFormat = true;
                    }
                }
                assert(foundFormat);
            }
        }

        VkPresentModeKHR surfacePresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        { // list all present modes
            uint32_t presentModeCount = 0;
            std::unique_ptr<VkPresentModeKHR[]> presentModes = nullptr;
            vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr);
            std::cout << presentModeCount << " present modes found:\n";
            if (presentModeCount > 0) {
                presentModes = std::make_unique<VkPresentModeKHR[]>(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, presentModes.get());

                for (int i = 0; i < presentModeCount; ++i) {
                    std::cout << "-> " << presentModes[i] << std::endl;
                    if (presentModes[i] == config::preferredPresentMode()) {
                        surfacePresentMode = presentModes[i];
                    }
                }
                assert(surfacePresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR);
            }
        }

        std::cout << std::endl;

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = _surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = _swapChainImageFormat.format;
        createInfo.imageColorSpace = _swapChainImageFormat.colorSpace;
        createInfo.imageExtent = _swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = surfacePresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapChain);
        assert(result == VK_SUCCESS);

        std::vector<VkImage> swapChainImages;
        uint32_t swapChainImageCount;
        vkGetSwapchainImagesKHR(_device, _swapChain, &swapChainImageCount, nullptr);
        swapChainImages.resize(swapChainImageCount);
        vkGetSwapchainImagesKHR(_device, _swapChain, &swapChainImageCount, swapChainImages.data());

        for (const auto& image : swapChainImages) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = _swapChainImageFormat.format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            _swapChainImageViews.push_back({});
            VkResult result = vkCreateImageView(_device, &createInfo, nullptr, &_swapChainImageViews.back());
            assert(result == VK_SUCCESS);
        }
    }
}

namespace scene {
    struct ShaderObjects {
    public:
        ShaderObjects(const std::string &vertFileName, const std::string &fragFileName) {
            vertShaderModule = utility::shaderModuleFromFile(vertFileName);
            fragShaderModule = utility::shaderModuleFromFile(fragFileName);

            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            _stages = { vertShaderStageInfo, fragShaderStageInfo };
        }

        ~ShaderObjects() {
            vkDestroyShaderModule(_device, vertShaderModule, nullptr);
            vkDestroyShaderModule(_device, fragShaderModule, nullptr);
        }

        uint32_t numStages() {
            return _stages.size();
        }

        const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages() {
            return _stages;
        }

    private:
        VkShaderModule vertShaderModule = VK_NULL_HANDLE;
        VkShaderModule fragShaderModule = VK_NULL_HANDLE;
        VkPipelineShaderStageCreateInfo vertShaderStageInfo;
        VkPipelineShaderStageCreateInfo fragShaderStageInfo;
        std::vector<VkPipelineShaderStageCreateInfo> _stages;
    };

    std::unique_ptr<ShaderObjects> _shaderObjects;
    VkPipelineLayout _pipelineLayout;
    VkRenderPass _renderPass;
    VkPipeline _graphicsPipeline;

    void createGraphicsPipeline() {
        _shaderObjects = std::make_unique<ShaderObjects>("triangle_vert.spv", "color_frag.spv");

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // no vertex data, yet
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // no vertex data, yet

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState = {};
        {
            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)_swapChainExtent.width;
            viewport.height = (float)_swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = _swapChainExtent;

            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;
        }

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // TODO: VkPipelineDepthStencilStateCreateInfo

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // TODO: VkPipelineDynamicStateCreateInfo

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        {
            VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
            assert(result == VK_SUCCESS);
        }

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = _shaderObjects->numStages();
        pipelineInfo.pStages = _shaderObjects->shaderStages().data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional
        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = _renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        {
            VkResult result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
            assert(result == VK_SUCCESS);
        }
    }

    void createRenderPass() {
        VkRenderPassCreateInfo renderPassInfo = {};
        {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = _swapChainImageFormat.format;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
        }

        VkResult result = vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass);
        assert(result == VK_SUCCESS);
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
        steps::setupDevice();
        steps::createSwapChain();
    }

    void shutdown() {
        for (auto imageView : _swapChainImageViews) {
            vkDestroyImageView(_device, imageView, nullptr);
        }
        _swapChainImageViews.clear();

        vkDestroySwapchainKHR(_device, _swapChain, nullptr);
        _swapChain = VK_NULL_HANDLE;

        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _surface = VK_NULL_HANDLE;

        debug_utils::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        _debugMessenger = VK_NULL_HANDLE;

        vkDestroyInstance(_instance, nullptr);
        _instance = VK_NULL_HANDLE;
    }

    void setupScene() {
        scene::createRenderPass();
        scene::createGraphicsPipeline();
    }

    void tearDownScene() {
        vkDestroyPipeline(_device, scene::_graphicsPipeline, nullptr);
        scene::_graphicsPipeline = VK_NULL_HANDLE;

        vkDestroyPipelineLayout(_device, scene::_pipelineLayout, nullptr);
        scene::_pipelineLayout = VK_NULL_HANDLE;

        vkDestroyRenderPass(_device, scene::_renderPass, nullptr);
        scene::_renderPass = VK_NULL_HANDLE;

        scene::_shaderObjects = nullptr;
    }
}
