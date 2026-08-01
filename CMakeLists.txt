cmake_minimum_required(VERSION 3.16)
project(BGL1 VERSION 0.1.0 LANGUAGES CXX C)

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
# The engine library — BGL1 (Bendy's Graphics Library)
# ----------------------------------------------------------------
add_library(BGL1 STATIC
    src/vkInit.cpp
    src/engine.cpp
    src/renderer.cpp
    src/shaders.cpp
    src/geometry.cpp
    src/memory.cpp
    src/input.cpp
    src/camera.cpp
    src/queues.cpp
    src/volkVma.cpp
)

target_include_directories(BGL1
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${vma_SOURCE_DIR}/include
    PRIVATE
        ${slang_SOURCE_DIR}/include
)

target_compile_definitions(BGL1
    PUBLIC
        GLFW_INCLUDE_NONE
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

target_compile_features(BGL1 PUBLIC cxx_std_20)

target_compile_options(BGL1 PRIVATE -O2)

target_link_libraries(BGL1
    PUBLIC
        glfw
        volk
        glm::glm
        tinyobjloader
        ${slang_SOURCE_DIR}/lib/libslang.so
)

# ----------------------------------------------------------------
# Demo executables — each is a standalone app with its own main, so they
# also serve as examples of using the library from your own project.
# vulkanEngine is the flagship executable (the 3D bunny demo).
# ----------------------------------------------------------------

add_executable(vulkanEngine src/demos/3DDemo.cpp src/demos/3DDemoMain.cpp)
target_link_libraries(vulkanEngine PRIVATE BGL1)

add_executable(funDemo src/demos/funDemo.cpp src/demos/funDemoMain.cpp)
target_link_libraries(funDemo PRIVATE BGL1)

add_executable(triangleDemo src/demos/triangleDemo.cpp src/demos/triangleDemoMain.cpp)
target_link_libraries(triangleDemo PRIVATE BGL1)

add_executable(windowOrbitDemo src/demos/windowOrbitDemo.cpp src/demos/windowOrbitDemoMain.cpp)
target_link_libraries(windowOrbitDemo PRIVATE BGL1)
