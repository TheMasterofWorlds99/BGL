#include "../include/renderer.hpp"
#include "../include/check.hpp"
#include "../include/engine.hpp"
#include <iostream>

/*

   renderer.cpp contains the implementations for renderer.hpp — the draw and
   compute helpers that sit on top of the engine's frame loop.

*/

void drawMesh(VkCommandBuffer cmdBuffer, const GPUMesh &mesh,
              const GraphicsShader &shader, uint32_t instanceCount,
              const void *pushData, uint32_t pushSize,
              const GPUBuffer *instanceBuffer, VkDescriptorSet descriptorSet) {
  // Bind pipeline
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    shader.pipeline);
  // Bind the descriptor set (e.g. a texture) if the shader uses one
  if (descriptorSet != VK_NULL_HANDLE) {
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            shader.pipelineLayout, 0, 1, &descriptorSet, 0,
                            nullptr);
  }
  if (pushData && pushSize > 0) {
    push(cmdBuffer, shader, pushSize, pushData);
  }
  // Bind vertex and index buffers
  VkDeviceSize offset{0};
  vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &mesh.vertexBuffer.vkBuffer, &offset);
  vkCmdBindIndexBuffer(cmdBuffer, mesh.indexBuffer.vkBuffer, offset,
                       VK_INDEX_TYPE_UINT32);
  // Bind the per-instance data as a second vertex buffer, if the
  // shader uses one
  if (instanceBuffer) {
    vkCmdBindVertexBuffers(cmdBuffer, 1, 1, &instanceBuffer->vkBuffer, &offset);
  }
  // Now we draw instanceCount number of mesh instances
  vkCmdDrawIndexed(cmdBuffer, mesh.indices.size(), instanceCount, 0, 0, 0);
}

// Simple barrier to ensure compute finishes before graphics reads the vertex
// buffer
void cmdComputeMemoryBarrier(VkCommandBuffer cmdBuffer) {
  VkMemoryBarrier2 memoryBarrier{
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
      .dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT};
  VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                  .memoryBarrierCount = 1,
                                  .pMemoryBarriers = &memoryBarrier};
  vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

void runCompute1D(VkCommandBuffer cmdBuffer, const ComputeKernel &compute,
                  uint32_t threadCount, const void *pushData,
                  uint32_t pushSize) {
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute.pipeline);

  if (pushData && pushSize > 0) {
    if (pushSize > compute.pushConstantSize) {
      std::cerr << "ERROR: Compute push constant data too big!\n";
      exit(EXIT_FAILURE);
    }
    vkCmdPushConstants(cmdBuffer, compute.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize, pushData);
  }

  uint32_t threadsPerGroup = compute.reflectionInfo.computeThreadGroupSizeX;
  uint32_t numGroups = (threadCount + threadsPerGroup - 1) / threadsPerGroup;

  vkCmdDispatch(cmdBuffer, numGroups, 1, 1);

  cmdComputeMemoryBarrier(cmdBuffer);
}

VkPipelineLayout createComputePipelineLayout(
    const Engine &engine, uint32_t pushConstantSize,
    const std::vector<VkDescriptorSetLayout> &setLayouts) {
  if (pushConstantSize > engine.maxPushConstantSize) {
    std::cerr << "ERROR: Requested " << pushConstantSize
              << " bytes of push constants, but GPU supports max "
              << engine.maxPushConstantSize << "\n";
    exit(EXIT_FAILURE);
  }

  VkPushConstantRange pushConstantRange{.stageFlags =
                                            VK_SHADER_STAGE_COMPUTE_BIT,
                                        .offset = 0,
                                        .size = pushConstantSize};

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
      .pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data(),
      .pushConstantRangeCount = pushConstantSize > 0 ? 1u : 0u,
      .pPushConstantRanges =
          pushConstantSize > 0 ? &pushConstantRange : nullptr};

  VkPipelineLayout layout;
  checkVk(vkCreatePipelineLayout(engine.gpu, &layoutInfo, nullptr, &layout),
          "Failed to create compute pipeline layout!");
  return layout;
}

VkPipeline createComputePipeline(const Engine &engine, VkPipelineLayout layout,
                                 VkShaderModule computeModule) {
  // Note: Slang renames every entry point to "main" in the emitted SPIR-V,
  // so we must reference it by that name here (same as the graphics path).
  VkPipelineShaderStageCreateInfo stageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeModule,
      .pName = "main"};

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = stageInfo,
      .layout = layout};

  VkPipeline pipeline;
  checkVk(vkCreateComputePipelines(engine.gpu, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &pipeline),
          "Failed to create compute pipeline!");
  return pipeline;
}

ComputeKernel createComputeKernel(const Engine &engine, const char *shaderPath,
                                  uint32_t pushConstantSize,
                                  const char *entryPointName,
                                  const std::vector<VkDescriptorSetLayout> &setLayouts) {
  VkShaderModule computeModule =
      compileShader(engine, shaderPath, entryPointName);
  VkPipelineLayout layout =
      createComputePipelineLayout(engine, pushConstantSize, setLayouts);
  VkPipeline pipeline = createComputePipeline(engine, layout, computeModule);
  vkDestroyShaderModule(engine.gpu, computeModule, nullptr);

  return ComputeKernel{
      .pipelineLayout = layout,
      .pipeline = pipeline,
      .reflectionInfo = {.computeThreadGroupSizeX =
                             64}, // Hardcoded for now until full reflection
      .pushConstantSize = pushConstantSize};
}
