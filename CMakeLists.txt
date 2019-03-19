cmake_minimum_required (VERSION 2.6)
project(HelloWorld)

# dependencies
set(VulkanSDKPath "${PROJECT_SOURCE_DIR}/vulkansdk-macos-1.1.101.0/macOS")
set(VulkanFrameworkPath "${VulkanSDKPath}/Frameworks/vulkan.framework")

# sources
set(SourceFiles
	src/main.cpp
)

add_executable(HelloWorld ${SourceFiles})

set_property(TARGET HelloWorld PROPERTY CXX_STANDARD 14)

target_compile_definitions(HelloWorld
    PRIVATE
        -DVK_ICD_FILENAMES="${PROJECT_SOURCE_DIR}/vulkansdk-macos-1.1.101.0/macOS/etc/vulkan/icd.d/MoltenVK_icd.json"
        -DVK_LAYER_PATH="${PROJECT_SOURCE_DIR}/vulkansdk-macos-1.1.101.0/macOS/etc/vulkan/explicit_layer.d"
)

target_include_directories(HelloWorld
	PRIVATE
		${VulkanSDKPath}/include
)

target_link_libraries(HelloWorld
    ${VulkanSDKPath}/lib/libvulkan.1.dylib
    # "-framework ${VulkanFrameworkPath}"
)
