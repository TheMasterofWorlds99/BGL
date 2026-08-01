#pragma once

// Includes
#include <cstdint>
#include <volk.h>

// volk before vma
#include <vk_mem_alloc.h>

/*

   memory.hpp will handle all the memory for the library. It will heavily
   utilize vma for the buffers and things we send to the GPU.

*/

// Here is a GPUBuffer struct to hold the information we'll send to the GPU,
// like triangle data, or a circle, etc.
struct GPUBuffer {
  VkBuffer vkBuffer;
  VmaAllocation alloc;
  VmaAllocationInfo allocInfo;
  uint32_t sizeBytes;
};

// Function which will allow us to create a GPU buffer, without filling it with
// data. Great for data we want to update every frame, like instanced positions
GPUBuffer createGPUBuffer(VmaAllocator allocator, VkBufferUsageFlags usageFlags,
                          uint32_t sizeBytes);

// Copy new data from the CPU into a GPU buffer
void uploadDataToGPUBuffer(VmaAllocator allocator, GPUBuffer &buffer,
                           const void *data, uint32_t sizeBytes);

// Function which will allow us to create the GPUBuffer, filling it with data
// right away
GPUBuffer createGPUBufferWithData(VmaAllocator allocator,
                                  VkBufferUsageFlags usageFlags,
                                  const void *data, uint32_t sizeBytes);

// Helper function to allow us to extact 64 bit pointer addresses from buffers
// we create
uint64_t getGPUBufferAddress(VkDevice device, VkBuffer buffer);

// Image barriers are Vulkan's way of telling the driver how we'll use the image
// next. This function allows us to tell vulkan to tell the driver what we're
// doing with the image
void swapchainImageBarrierCmd(VkCommandBuffer cmdBuffer, VkImage image);

// Little helper function to see if the format includes a stencil component
bool hasStencilComponent(VkFormat format);

// Barrier to transition depth image layout for rendering
void depthImageBarrierCmd(VkCommandBuffer cmdBuffer, VkImage image,
                          VkFormat format);

// Unlike the first barrier, we're now telling vulkan to tell the driver, we're
// done with the image, the presentation engine can now read it and use it
void addPresentationMemoryBarrierCmd(VkImage image, VkCommandBuffer cmdBuffer);
