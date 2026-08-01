#pragma once

// Include
#include "../engine.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

// Gloal debug flag to turn things on and off
#ifdef NDEBUG
constexpr bool DebugEnabled = false;
#else
constexpr bool DebugEnabled = true;
#endif

/*

   vkInit.hpp handles all the initalization of vulkan and all the checks,
   whilst also sending data to engine to handle the rest. It simply contains
   all the initalization functions that will be run in main.cpp, where these
   functions take Engine as a reference

*/

// We must first create a Vulkan Instance for engine. During this, we can also
// check GLFW, ensure volk is working, and create the debugutilmessenger for
// engine as well
void createVulkanInstance(Engine &engine);

// Now with the vulkan instance created, we can create the GLFW window and the
// vulkan surface
void createWindowAndSurface(Engine &engine, const char *title, int32_t width,
                            int32_t height);

// After inializing a vk instance, along with our window and surface, we can
// then select our device. For simplicity we'll by default look for discrete
// gpu's and if none are found, chose the first device that works
VkPhysicalDevice selectPhysicalDevice(Engine &engine);

// With the selectGPU function, we can now create a logical device to
// communicate with it
void createLogicalGPUDeviceAndQueue(Engine &engine);

void initVMA(Engine &engine);

// Create the command pool for window
void createCommandPool(Engine &engine);

// Create the syncronization objects for the window
void createSyncObjects(const Engine &engine, Window &window);

// With the GPUs, families, and command buffer in place, we can then setup the
// swapchain
void createSwapChain(const Engine &engine, Window &window);

// Next we initalize the swapchain images and imageviews
void createSwapchainImagesAndViews(const Engine &engine, Window &window);

// Function to create the depth image
void createDepthAndStencilImage(const Engine &engine, Window &window);

// One-stop setup: instance, window(s), GPU, queues, VMA and all per-window
// swapchain resources. The single call every demo main makes before run().
void initEngine(Engine &engine, const char *title, int32_t width,
                int32_t height, uint32_t windowCount = 1);

// This function cleans up the entire engine
void cleanupEngine(Engine &engine);
