#include "../include/queues.hpp"
#include "../include/check.hpp"
#include "../include/engine.hpp"
#include <vector>

/*

   queues.cpp contains the implementations for queues.hpp — finding the GPU's
   graphics queue family and allocating command buffers from the engine's
   command pool.

*/

// The GPU has a bunch of queue families that we want to look through. The goal
// is to find one that allows us to send graphics commands through it. This
// function simply takes the physical GPU, and searchs through it's queue
// families to find it
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
  // Setup the struct that will hold the anwser to the right queue
  QueueFamilyIndices indicies;

  // Start a count that vulkan can send data to (vk found 3 families, as an
  // example, so this would turn to 3)
  uint32_t queueFamilyCount = 0;

  // Find the number of families here
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  // Now that we have the number of families, allocate memory to a vector to
  // rerun the above function to get the actual properties of the families
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  // Now we search through all of the families in the buffer to find one that
  // can take our graphics commands!
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    // if we found a family that can take the graphics, as seen in the if
    // statement, we're good!
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indicies.graphicsFamilyIndex = i;
      indicies.foundGraphics = true;
      break;
    }
  }

  return indicies;
}

// This is a function to create command buffers which we'll send to the GPU to
// draw stuff
void createCommandBuffers(const Engine &engine, Window &window) {
  // Create command buffers for the pool (1 for each frame in flight)
  const VkCommandBufferAllocateInfo cmdBufferAllocationInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = engine.commandPool,
      .commandBufferCount = MaxFramesInFlight};

  // Command buffers are stored per window, to allow for multi window setups
  checkVk(vkAllocateCommandBuffers(engine.gpu, &cmdBufferAllocationInfo,
                                   window.commandBuffers.data()),
          "Failed to allocate command buffer!");
}
