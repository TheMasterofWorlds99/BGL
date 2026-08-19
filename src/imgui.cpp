#include "../include/imgui.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void initImgui(Engine &engine, Window &window) {
  // First we create our context
  ImGui::CreateContext();

  // Next we initalize imgui for glfw and vulkan
  ImGui_ImplGlfw_InitForVulkan(
      window.glfwWindow, false); // False to ensure ImGui uses our input system

  // Now we setup our info
  ImGui_ImplVulkan_InitInfo initInfo{
      .Instance = engine.vulkanInstance,
      .PhysicalDevice = engine.physicalGPU,
      .Device = engine.gpu,
      .QueueFamily = engine.queueFamilyIndex,
      .Queue = engine.queue,
      .MinImageCount = static_cast<uint32_t>(window.images.size()),
      .ImageCount = static_cast<uint32_t>(window.images.size()),
      .MSAASamples = engine.settings.MSAASamples,
      .DescriptorPoolSize = 1000,
      .UseDynamicRendering = true,
  };

  initInfo.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &engine.activeWindow->swapchainFormat,
      .depthAttachmentFormat =
          window.DepthImageFormat, // must match your render pass
  };

  ImGui_ImplVulkan_Init(&initInfo);
}

void imguiBeginFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void imguiRender(VkCommandBuffer cmdBuffer) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

void cleanupImgui() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
