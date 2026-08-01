cmake_minimum_required(VERSION 3.16)
project(triangle VERSION 0.1.0 LANGUAGES CXX C)

# Default to an optimized build. Without this the sim runs ~40x slower than it
# should (GCC defaults to -O0). Override anytime with -DCMAKE_BUILD_TYPE=Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG master
)

FetchContent_Declare(
    vk_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG master
)

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG master
)

FetchContent_Declare(
    slang
    URL https://github.com/shader-slang/slang/releases/download/v2024.14.3/slang-2024.14.3-linux-x86_64.zip
)

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG release
)

FetchContent_MakeAvailable(
    glfw
    volk
    vk_headers
    vma
    glm
    slang
    tinyobjloader
)

# ----------------------------------------------------------------
# Compiler executable
# ----------------------------------------------------------------

add_executable(triangle src/main.cpp
                        src/demos/windowOrbitDemo.cpp
                        src/demos/triangleDemo.cpp
                        src/demos/funDemo.cpp
                        src/demos/3DDemo.cpp
)

target_compile_options(triangle PRIVATE -O2)

target_include_directories(triangle
    PRIVATE
        ${vma_SOURCE_DIR}/include
        ${slang_SOURCE_DIR}/include
)

target_compile_definitions(triangle
    PRIVATE
        GLFW_INCLUDE_NONE
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

target_compile_features(triangle PRIVATE cxx_std_20)

target_link_libraries(triangle
    PRIVATE
        glfw
        volk
        glm::glm
        tinyobjloader
        ${slang_SOURCE_DIR}/lib/libslang.so
)
