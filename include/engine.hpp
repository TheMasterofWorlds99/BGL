#pragma once

// Volk before glfw
#include <volk.h>

// VMA after volk
#include <vk_mem_alloc.h>

// Other Includes
#include "memory.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

/*

   Engine.hpp holds the primary engine class, which will handle all the vulkan
   functions, glfw windows, etc.

*/

// -- IMPORTANT CONSTANTS --
constexpr uint32_t MaxFramesInFlight = 2;
constexpr VkFormat SwapchainImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
constexpr uint64_t Timeout = UINT64_MAX;

// Modifiable Settings
inline bool CullBackFace = true;

// Basic Window struct helper for the engine struct
struct Window {
  GLFWwindow *glfwWindow; // native window
  VkSurfaceKHR surface;   // Abstraction over native window
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  VkSwapchainKHR swapchain; // Provides access to images (via presentation
                            // engine) to render to
  VkExtent2D swapchainExtent;

  std::vector<VkImage> images; // swapchain images
  std::vector<VkImageView> imageViews;

  // Depth image format
  VkFormat DepthImageFormat = VK_FORMAT_D32_SFLOAT;

  VkImage depthImage;
  VkImageView depthImageView;
  VmaAllocation depthAlloc;

  // Index of image to target for rendering (can be out of order, so request
  // from swapchain)
  uint32_t activeImageIndex;
  // Current index for reusable frame resources (cmd buffers, syncronization
  // objects) If, for example, 2 frames in flight are used, this value will go:
  // 0, 1, 0, 1, etc.
  uint32_t frameInFlightIndex;

  // Command buffers
  std::array<VkCommandBuffer, MaxFramesInFlight> commandBuffers;

  // -- Synchronization objects for presentation
  // Signalled by presentation engine whem image is available for gpu to start
  // rendering
  std::array<VkSemaphore, MaxFramesInFlight> imageAcquiredSemaphore;
  // Signalled by gpu once commands have been executed, meaning the image can be
  // presented
  std::vector<VkSemaphore> presentationReadySemaphore; // Once per image
  // Signalled by gpu once commands have been executed, meaning the cpu can
  // reuse that frame's resources
  std::array<VkFence, MaxFramesInFlight> frameFinishedFence;
};

// Window helpers
inline uint32_t windowWidth(const Window *window) {
  int width, height;
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);
  return static_cast<uint32_t>(width);
}

inline uint32_t windowHeight(const Window *window) {
  int width, height;
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);
  return static_cast<uint32_t>(height);
}

// GPU Features struct for handling the tons of features vulkan can offer
struct GPUFeatures {
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
  };

  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13, // Chain it to the 1.3 features!
      .bufferDeviceAddress = VK_TRUE,
  };
};

// Engine structure
struct Engine {
public:
  std::vector<Window>
      windows; // A list of windows that Engine will be able to handle

  Window *activeWindow = nullptr; // Window the engine is focusing on

  // Vulkan types that engine would use
  VkInstance vulkanInstance;               // Vulkan Instance
  VkDebugUtilsMessengerEXT debugMessenger; // debugMessenger

  // Vulkan GPUs
  VkPhysicalDevice physicalGPU;
  VkDevice gpu;

  // Family Queue Stuff
  uint32_t queueFamilyIndex;
  VkQueue queue;

  // Command Stuff
  VkCommandPool commandPool;

  // VMA Allocator
  VmaAllocator allocator;

  // A quick push constant size holder so no matter the GPU, the engine knows
  // the maximum gaurenteed memory that it can use for push constants
  uint32_t maxPushConstantSize =
      128; // By default, vulkan only supports a maximum of 128 bytes

private:
};

// Helper Functions for rendering and framing
inline void viewportAndScissorCmd(VkCommandBuffer cmdBuffer, Window *window) {
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

// Rendering and Frame functions
inline void beginRenderPass(VkCommandBuffer cmdBuffer, Window *window,
                            glm::vec4 col) {

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

// Frame functions
// Prepare the frame: wait for the frame-in-flight fence, acquire the next
// swapchain image, and reset/begin the command buffer for recording. The
// render pass is NOT started, so callers that need to record commands before
// rendering (e.g. a compute pass) can do so, then call beginRenderPass.
inline VkCommandBuffer beginFrameCommandBuffer(const Engine &engine,
                                               Window *window) {
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

inline void beginFrame(const Engine &engine, Window *window, glm::vec4 col) {
  VkCommandBuffer cmdBuffer = beginFrameCommandBuffer(engine, window);
  // Begin render pass so we can start recording graphics commands
  beginRenderPass(cmdBuffer, window, col);
}

inline void submitCommands(VkQueue queue, VkCommandBuffer cmdBuffer,
                           Window *window) {
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

inline void endFrame(const Engine &engine, Window *window) {
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
