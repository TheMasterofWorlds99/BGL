#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../include/demos/3DDemo.hpp"
#include "../include/engine.hpp"
#include "../include/vulkanInit/vkInit.hpp"
#include <iostream>

int main() {
  std::cout << "3D test!\n";
  Engine engine;

  // Initalization
  createVulkanInstance(engine);

  // Create the planet and moon windows
  createWindowAndSurface(engine, "Breath taking 3D visuals", 1280, 720);

  // Initalize the GPU, Queues and Swapchains for all windows
  engine.physicalGPU = selectPhysicalDevice(engine);
  createLogicalGPUDeviceAndQueue(engine);
  initVMA(engine);
  createCommandPool(engine);

  for (auto &window : engine.windows) {
    createSwapChain(engine, window);
    createSwapchainImagesAndViews(engine, window);
    createDepthAndStencilImage(engine, window);
    createCommandBuffers(engine, window);
    createSyncObjects(engine, window);
  }

  Demo3D::run(engine);

  std::cout << "Cleaning up engine resources...\n";
  cleanupEngine(engine);

  return 0;
}
