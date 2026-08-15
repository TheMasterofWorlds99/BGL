#include "../include/texture.hpp"
#include <memory.hpp>

Texture createTexture(Engine &engine, uint32_t width, uint32_t height,
                      const uint8_t *pixels) {
  // Let's create our VkImage Info
  VkImageCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {width, height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo allocInfo{
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };

  Texture texture{};

  vmaCreateImage(engine.allocator, &info, &allocInfo, &texture.image,
                 &texture.alloc, 0);

  texture.width = width;
  texture.height = height;

  GPUBuffer staging = createGPUBuffer(
      engine.allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, width * height * 4);
  uploadDataToGPUBuffer(engine.allocator, staging, pixels, width * height * 4);

  VkCommandBufferAllocateInfo allocInfo2{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = engine.commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(engine.gpu, &allocInfo2, &cmd);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  vkBeginCommandBuffer(cmd, &beginInfo);

  VkImageMemoryBarrier b1{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = texture.image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &b1);

  VkBufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
                           1}, // {aspect, mip, layer, layerCount}
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};
  vkCmdCopyBufferToImage(cmd, staging.vkBuffer, texture.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  VkImageMemoryBarrier b2 = b1;
  b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &b2);

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &cmd};
  VkFence fence;
  VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  vkCreateFence(engine.gpu, &fenceInfo, nullptr, &fence);
  vkQueueSubmit(engine.queue, 1, &submitInfo, fence);
  vkWaitForFences(engine.gpu, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(engine.gpu, fence, nullptr);
  vkFreeCommandBuffers(engine.gpu, engine.commandPool, 1, &cmd);

  vmaDestroyBuffer(engine.allocator, staging.vkBuffer, staging.alloc);

  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = texture.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCreateImageView(engine.gpu, &viewInfo, nullptr, &texture.view);

  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .maxLod = 1.0f};
  vkCreateSampler(engine.gpu, &samplerInfo, nullptr, &texture.sampler);

  return texture;
}

void destroyTexture(Engine &engine, Texture &texture) {
  vkDestroySampler(engine.gpu, texture.sampler, nullptr);
  vkDestroyImageView(engine.gpu, texture.view, nullptr);
  vmaDestroyImage(engine.allocator, texture.image, texture.alloc);
}
