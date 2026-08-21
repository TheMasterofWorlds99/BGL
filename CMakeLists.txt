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
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG release
)

FetchContent_Declare(
  tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG v2.9.5
)

FetchContent_MakeAvailable(
    glfw
    volk
    vk_headers
    vma
    glm
    slang
    tinyobjloader
    tinygltf
)

# Setup ImGui library
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
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
    src/imgui.cpp
    src/texture.cpp
    src/tinygltf.cpp
)

target_include_directories(BGL1
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${vma_SOURCE_DIR}/include
    ${tinygltf_SOURCE_DIR}
  PRIVATE
    ${slang_SOURCE_DIR}/include
)

target_include_directories(imgui
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(BGL1
  PUBLIC
    GLFW_INCLUDE_NONE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)

target_compile_definitions(imgui
  PRIVATE
    IMGUI_IMPL_VULKAN_USE_VOLK
)

target_compile_features(BGL1 PUBLIC cxx_std_20)

target_compile_options(BGL1 PRIVATE -O2)

target_link_libraries(BGL1
    PUBLIC
        glfw
        volk
        glm::glm
        imgui
        tinyobjloader
        ${slang_SOURCE_DIR}/lib/libslang.so
)

target_link_libraries(imgui
  PUBLIC
    glfw
    volk
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

add_executable(textureTest src/demos/textureTest.cpp src/demos/textureTestMain.cpp)
target_link_libraries(textureTest PRIVATE BGL1)

add_executable(glbTest src/demos/glbTest.cpp src/demos/glbTestMain.cpp)
target_link_libraries(glbTest PRIVATE BGL1)

add_executable(galleryDemo src/demos/galleryDemo.cpp src/demos/galleryDemoMain.cpp)
target_link_libraries(galleryDemo PRIVATE BGL1)
