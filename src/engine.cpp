#include "../include/engine.hpp"
#include "../include/check.hpp"
#include "../include/memory.hpp"

/*

   engine.cpp contains the implementations for engine.hpp — the frame loop
   helpers (acquiring images, recording command buffers, rendering passes,
   submitting and presenting).

*/

void tickTimer(Engine &engine) {
  double now = glfwGetTime();

  // First call: no previous time, so no delta
  if (engine.globalTimer.lastTime < 0.0) {
    engine.globalTimer.deltaTime = 0.0f;
  } else {
    engine.globalTimer.deltaTime =
        static_cast<float>(now - engine.globalTimer.lastTime);
  }
  engine.globalTimer.lastTime = now;
  engine.globalTimer.tickCount++;
}

void viewportAndScissorCmd(VkCommandBuffer cmdBuffer, Window *window) {
  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(window->swapchainExtent.width),
      .height = static_cast<float>(window->swapchainExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

  VkRect2D scissor{.offset = {0, 0}, .extent = window->swapchainExtent};

  vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}

void beginRenderPass(VkCommandBuffer cmdBuffer, Window *window, glm::vec4 col) {
  VkRenderingAttachmentInfo colAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = window->imageViews[window->activeImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue{.color = {col.r, col.g, col.b, col.a}}};

  // Configure Depth Attachment
  VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = window->depthImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .clearValue{.depthStencil = {1.0f, 0}} // 1.0 is the max depth
  };

  VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                .renderArea{.extent{window->swapchainExtent}},
                                .layerCount = 1,
                                .colorAttachmentCount = 1,
                                .pColorAttachments = &colAttachmentInfo,
                                .pDepthAttachment = &depthAttachmentInfo};

  swapchainImageBarrierCmd(cmdBuffer, window->images[window->activeImageIndex]);

  // Transition the depth image layout
  depthImageBarrierCmd(cmdBuffer, window->depthImage, window->DepthImageFormat);

  vkCmdBeginRendering(cmdBuffer, &renderingInfo);
  viewportAndScissorCmd(cmdBuffer, window);
}

VkCommandBuffer beginFrameCommandBuffer(const Engine &engine, Window *window) {
  // Wait until GPU has finished all previosuly submitted commands for this
  // frame-in-flight
  VkFence *frameFence = &window->frameFinishedFence[window->frameInFlightIndex];
  checkVk(vkWaitForFences(engine.gpu, 1, frameFence, true, Timeout),
          "Failed to wait for frame fence");
  checkVk(vkResetFences(engine.gpu, 1, frameFence),
          "Failed to resest frame fence"); // reset fence for next use

  // Acquire next swapchain image
  VkSemaphore acquiredFlag =
      window->imageAcquiredSemaphore[window->frameInFlightIndex];
  checkVk(vkAcquireNextImageKHR(engine.gpu, window->swapchain, Timeout,
                                acquiredFlag, nullptr,
                                &window->activeImageIndex),
          "Failed to acquire next swapchain image");

  VkCommandBufferBeginInfo cmdBufferBeginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkCommandBuffer cmdBuffer =
      window->commandBuffers[window->frameInFlightIndex];
  checkVk(vkResetCommandBuffer(cmdBuffer, 0), "Failed to reset command buffer");
  checkVk(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo),
          "Failed to begin recording command buffer");

  return cmdBuffer;
}

void beginFrame(const Engine &engine, Window *window, glm::vec4 col) {
  VkCommandBuffer cmdBuffer = beginFrameCommandBuffer(engine, window);
  // Begin render pass so we can start recording graphics commands
  beginRenderPass(cmdBuffer, window, col);
}

void submitCommands(VkQueue queue, VkCommandBuffer cmdBuffer, Window *window) {
  // GPU must wait for image acquisiton before executing cmds of this stage
  // (in which first colors are output). Earlier stages (like dealing with
  // verticies for example) can start executing right away though.
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &window->imageAcquiredSemaphore[window->frameInFlightIndex],
      .pWaitDstStageMask = &waitStage, // Wait for above flag before exeuctig
                                       // this (and later) stages
      .commandBufferCount = 1,         // Can submit multiple in case we want to
                                       // record commands from different threads
      .pCommandBuffers = &cmdBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &window->presentationReadySemaphore
                                [window->activeImageIndex], // Signal when done
  };

  // Submit queue with given command buffer and signal fence when finished
  checkVk(vkQueueSubmit(queue, 1, &submitInfo,
                        window->frameFinishedFence[window->frameInFlightIndex]),
          "Failed to submit command buffer to graphics queue");
}

void endFrame(const Engine &engine, Window *window) {
  // End render pass and sumbit commands
  VkCommandBuffer cmdBuffer =
      window->commandBuffers[window->frameInFlightIndex];
  vkCmdEndRendering(cmdBuffer);
  addPresentationMemoryBarrierCmd(window->images[window->activeImageIndex],
                                  cmdBuffer);
  checkVk(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
  submitCommands(engine.queue, cmdBuffer, window);

  // Present image once rendering is complete (just issuing command; cpu
  // doesn't wait)
  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &window->presentationReadySemaphore[window->activeImageIndex],
      .swapchainCount = 1,
      .pSwapchains = &window->swapchain,
      .pImageIndices = &window->activeImageIndex};

  checkVk(vkQueuePresentKHR(engine.queue, &presentInfo),
          "Failed to present swapchain image");

  // Update index for next frame
  window->frameInFlightIndex =
      (window->frameInFlightIndex + 1) % MaxFramesInFlight;
}
