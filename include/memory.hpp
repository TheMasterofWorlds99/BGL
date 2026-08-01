#pragma once

// Includes
#include <cstring>
#include <volk.h>

#include "check.hpp"
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
inline GPUBuffer createGPUBuffer(VmaAllocator allocator,
                                 VkBufferUsageFlags usageFlags,
                                 uint32_t sizeBytes) {
  // NOTE: Usage flags specify thins like whether the buffer will be used for a
  // specialized purpose (such as vertex or index buffers), or whether it can be
  // written to from a compute shader, copied from, etc.
  VkBufferCreateInfo bufferCreateInfo{.sType =
                                          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = sizeBytes,
                                      .usage = usageFlags};

  // Now we create the allocation info which will allow us to allocate memory
  // that the cpu can write to. Ideally this memory will be on the graphics
  // card, but this does require certain things for the cpu to write to.
  // A later alternative approach would be to have the gpu itself copy data over
  // from the cpu (via a staging buffer)
  VmaAllocationCreateInfo allocCreateInfo{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO};

  GPUBuffer buffer{.sizeBytes = sizeBytes};
  checkVk(vmaCreateBuffer(allocator, &bufferCreateInfo, &allocCreateInfo,
                          &buffer.vkBuffer, &buffer.alloc, &buffer.allocInfo),
          "VMA failed to create GPU buffer!");

  return buffer;
}

// Copy new data from the CPU into a GPU buffer
inline void uploadDataToGPUBuffer(VmaAllocator allocator, GPUBuffer &buffer,
                                  const void *data, uint32_t sizeBytes) {
  if (sizeBytes > buffer.sizeBytes) {
    std::cerr << "ERROR: Trying to upload " << sizeBytes << " bytes to a "
              << buffer.sizeBytes << " byte GPU buffer!\n";
    exit(EXIT_FAILURE);
  }

  // Copy the data into the memory we mapped when creating the buffer
  memcpy(buffer.allocInfo.pMappedData, data, sizeBytes);
  checkVk(vmaFlushAllocation(allocator, buffer.alloc, 0, sizeBytes),
          "VMA failed to flush allocation!");
}

// Function which will allow us to create the GPUBuffer, filling it with data
// right away
inline GPUBuffer createGPUBufferWithData(VmaAllocator allocator,
                                         VkBufferUsageFlags usageFlags,
                                         const void *data, uint32_t sizeBytes) {
  GPUBuffer buffer = createGPUBuffer(allocator, usageFlags, sizeBytes);
  uploadDataToGPUBuffer(allocator, buffer, data, sizeBytes);
  return buffer;
}

// Helper function to allow us to extact 64 bit pointer addresses from buffers
// we create
inline uint64_t getGPUBufferAddress(VkDevice device, VkBuffer buffer) {
  VkBufferDeviceAddressInfo addressInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
  return vkGetBufferDeviceAddress(device, &addressInfo);
}

// Image barriers are Vulkan's way of telling the driver how we'll use the image
// next. This function allows us to tell vulkan to tell the driver what we're
// doing with the image
inline void swapchainImageBarrierCmd(VkCommandBuffer cmdBuffer, VkImage image) {
  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

      .image = image,

      .subresourceRange{
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

// Little helper function to see if the format includes a stencil component
inline bool hasStencilComponent(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// Barrier to transition depth image layout for rendering
inline void depthImageBarrierCmd(VkCommandBuffer cmdBuffer, VkImage image,
                                 VkFormat format) {
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (hasStencilComponent(format)) {
    aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // Undefined is fine because we
                                              // clear it every frame
      .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange{
          .aspectMask = aspectMask,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  vkCmdPipelineBarrier(cmdBuffer,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Unlike the first barrier, we're now telling vulkan to tell the driver, we're
// done with the image, the presentation engine can now read it and use it
inline void addPresentationMemoryBarrierCmd(VkImage image,
                                            VkCommandBuffer cmdBuffer) {

  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = 0,

      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

      .image = image,

      .subresourceRange{
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}
