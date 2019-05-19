cmake_minimum_required (VERSION 3.14.0)
project(HelloWorld)

# dependencies
set(VulkanSDKPath "${PROJECT_SOURCE_DIR}/vulkansdk-macos-1.1.106.0/macOS")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "GLFW's build shared library option")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW's build examples option")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW's build tests option")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW's build docs option")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw" glfw)

# sources
set(SourceFiles
    "${CMAKE_CURRENT_SOURCE_DIR}/src/glfw_integration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_integration.cpp"
)

add_executable(HelloWorld ${SourceFiles})

set_target_properties(HelloWorld
    PROPERTIES
        CXX_STANDARD 14
)

target_compile_definitions(HelloWorld
    PRIVATE
        -DVK_ICD_FILENAMES="${VulkanSDKPath}/etc/vulkan/icd.d/MoltenVK_icd.json"
        -DVK_LAYER_PATH="${VulkanSDKPath}/etc/vulkan/explicit_layer.d"
)

target_include_directories(HelloWorld
    PRIVATE
        ${VulkanSDKPath}/include
)

target_link_libraries(HelloWorld
    ${VulkanSDKPath}/lib/libvulkan.dylib
    glfw
)

add_custom_target(Shaders
    COMMAND "${VulkanSDKPath}/bin/glslangValidator" -V "${CMAKE_CURRENT_SOURCE_DIR}/resources/triangle.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/triangle_vert.spv"
    COMMAND "${VulkanSDKPath}/bin/glslangValidator" -V "${CMAKE_CURRENT_SOURCE_DIR}/resources/color.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/color_frag.spv"
)

add_dependencies(HelloWorld Shaders)
