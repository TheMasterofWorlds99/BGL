#pragma once

// Includes
#include "engine.hpp"
#include "geometry.hpp"
#include <cstddef>
#include <cstdlib>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vulkan/vulkan_core.h>

/*
   shaders.hpp is a header that will contain all the functions for the graphics
   library to compile the slang shaders users would provide, for use in projects
   and such
*/

constexpr VkFormat VK_FLOAT4 = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat VK_FLOAT3 = VK_FORMAT_R32G32B32_SFLOAT;
constexpr VkFormat VK_FLOAT2 = VK_FORMAT_R32G32_SFLOAT;
constexpr VkFormat VK_FLOAT1 = VK_FORMAT_R32_SFLOAT;

// We do need a graphicsshader structure so we can hold pipeline data for
// vulkan to process
struct GraphicsShader {
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkShaderStageFlags pushConstantStages = 0;
  uint32_t pushConstantSize = 0;
};

// This function will allow us to compile the Slang into spir-v and put it in a
// VkShaderModule
inline VkShaderModule compileShader(const Engine &engine,
                                    const char *shaderPath,
                                    const char *entryPointName) {
  using namespace Slang;
  using namespace slang;

  // We first create our global session
  ComPtr<IGlobalSession> slangGlobalSession;
  createGlobalSession(slangGlobalSession.writeRef());

  // Setup the target (We want spir-v for vulkan)
  TargetDesc targetDesc{.format = SLANG_SPIRV,
                        .profile =
                            slangGlobalSession->findProfile("spirv_1_6")};

  SessionDesc sessiocDesc{.targets = &targetDesc, .targetCount = 1};

  ComPtr<ISession> session;
  slangGlobalSession->createSession(sessiocDesc, session.writeRef());

  // Next we load the slang file
  ComPtr<IBlob> diagnosticsBlob;
  IModule *slangModule =
      session->loadModule(shaderPath, diagnosticsBlob.writeRef());
  if (diagnosticsBlob) {
    std::cerr << "Slang Compiler Error:\n"
              << (const char *)diagnosticsBlob->getBufferPointer() << "\n";
  }
  if (!slangModule)
    exit(EXIT_FAILURE);

  // Find the entry point (VSMain or FSMain)
  ComPtr<IEntryPoint> entryPoint;
  slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef());

  // If the module failed to compile there is no entry point to link; crash
  // cleanly with a useful message instead of segfaulting later
  if (!entryPoint) {
    std::cerr << "ERROR: Could not find entry point '" << entryPointName
              << "' in " << shaderPath
              << " (check the shader for compile errors)\n";
    exit(EXIT_FAILURE);
  }

  // Link the program and generate spir-v bytecode
  IComponentType *componentTypes[] = {slangModule, entryPoint};
  ComPtr<IComponentType> linkedProgram;
  session->createCompositeComponentType(componentTypes, 2,
                                        linkedProgram.writeRef());

  ComPtr<IBlob> spirvCode;
  linkedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(),
                                   diagnosticsBlob.writeRef());

  // Put the spir-v bytecode into a vulkan shader module
  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spirvCode->getBufferSize(),
      .pCode =
          reinterpret_cast<const uint32_t *>(spirvCode->getBufferPointer())};

  VkShaderModule shaderModule;
  checkVk(vkCreateShaderModule(engine.gpu, &createInfo, nullptr, &shaderModule),
          "Failed to create shader module!");

  return shaderModule;
}

// Create a descriptor set layout with a single storage buffer binding. This
// lets shaders read per-instance (or per-object) data uploaded from the CPU
inline VkDescriptorSetLayout createStorageBufferDescriptorSetLayout(
    const Engine &engine, VkShaderStageFlags stageFlags, uint32_t binding = 0) {
  VkDescriptorSetLayoutBinding layoutBinding{
      .binding = binding,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = stageFlags};

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &layoutBinding};

  VkDescriptorSetLayout layout;
  checkVk(
      vkCreateDescriptorSetLayout(engine.gpu, &layoutInfo, nullptr, &layout),
      "Failed to create descriptor set layout!");
  return layout;
}

// Create a descriptor pool big enough to allocate 'maxSets' sets, where each
// set holds one storage buffer descriptor
inline VkDescriptorPool createDescriptorPool(const Engine &engine,
                                             uint32_t maxSets) {
  VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .descriptorCount = maxSets};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = maxSets,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize};

  VkDescriptorPool pool;
  checkVk(vkCreateDescriptorPool(engine.gpu, &poolInfo, nullptr, &pool),
          "Failed to create descriptor pool!");
  return pool;
}

// Allocate a single descriptor set out of the pool, matching the given layout
inline VkDescriptorSet allocateDescriptorSet(const Engine &engine,
                                             VkDescriptorPool pool,
                                             VkDescriptorSetLayout layout) {
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout};

  VkDescriptorSet set;
  checkVk(vkAllocateDescriptorSets(engine.gpu, &allocInfo, &set),
          "Failed to allocate descriptor set!");
  return set;
}

// Point the storage buffer descriptor in 'set' at a GPU buffer, so the shader
// can read its contents
inline void updateStorageBufferDescriptorSet(const Engine &engine,
                                             VkDescriptorSet set,
                                             const GPUBuffer &buffer) {
  VkDescriptorBufferInfo bufferInfo{
      .buffer = buffer.vkBuffer, .offset = 0, .range = buffer.sizeBytes};

  VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = set,
                             .dstBinding = 0,
                             .descriptorCount = 1,
                             .descriptorType =
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .pBufferInfo = &bufferInfo};

  vkUpdateDescriptorSets(engine.gpu, 1, &write, 0, nullptr);
}

// Create the Pipeline Layout (Empty for now, used later for pushing
// matrices/textures to GPU)
inline VkPipelineLayout createGraphicsPipelineLayout(
    const Engine &engine, uint32_t pushConstantSize = 0,
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT,
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE) {
  if (pushConstantSize > engine.maxPushConstantSize) {
    std::cerr << "ERROR: Requested " << pushConstantSize
              << " bytes of push constants, but this GPU only supports "
              << engine.maxPushConstantSize << " bytes.\n";
    exit(EXIT_FAILURE);
  }

  VkPushConstantRange pushConstantRange{
      .stageFlags = pushConstantStages, .offset = 0, .size = pushConstantSize};

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = descriptorSetLayout != VK_NULL_HANDLE ? 1u : 0u,
      .pSetLayouts = descriptorSetLayout != VK_NULL_HANDLE
                         ? &descriptorSetLayout
                         : nullptr,
      .pushConstantRangeCount = pushConstantSize > 0 ? 1u : 0u,
      .pPushConstantRanges =
          pushConstantSize > 0 ? &pushConstantRange : nullptr};

  VkPipelineLayout layout;
  checkVk(vkCreatePipelineLayout(engine.gpu, &layoutInfo, nullptr, &layout),
          "Failed to create pipeline layout!");
  return layout;
}

// This function will create the graphics pipeline!
inline VkPipeline createGraphicsPipeline(
    const Engine &engine, VkPipelineLayout layout, VkShaderModule vertexModule,
    VkShaderModule fragmentModule,
    // Optional second vertex binding for per-instance data (e.g. instance
    // position/rotation), stepped once per instance instead of once per
    // vertex. Left empty for shaders that don't use instancing.
    const std::vector<VkVertexInputBindingDescription> &instanceBindings = {},
    const std::vector<VkVertexInputAttributeDescription> &instanceAttributes =
        {}) {
  // We first make the shader stage infos
  VkPipelineShaderStageCreateInfo vertStageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertexModule,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo fragStageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragmentModule,
      .pName = "main"};
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo,
                                                    fragStageInfo};

  // Now we connect our vertex struct to slang's VSInput
  VkVertexInputBindingDescription bindingDesc{.binding = 0,
                                              .stride = sizeof(Vertex),
                                              .inputRate =
                                                  VK_VERTEX_INPUT_RATE_VERTEX};

  std::vector<VkVertexInputAttributeDescription> vertexAttributes{
      {.location = 0,
       .binding = 0,
       .format = VK_FLOAT3,
       .offset = offsetof(Vertex, pos)},
      {.location = 1,
       .binding = 0,
       .format = VK_FLOAT3,
       .offset = offsetof(Vertex, normal)},
      {.location = 2,
       .binding = 0,
       .format = VK_FLOAT2,
       .offset = offsetof(Vertex, uv)},
  };

  // Combine the per-vertex binding/attributes with any per-instance ones
  std::vector<VkVertexInputBindingDescription> bindings{bindingDesc};
  bindings.insert(bindings.end(), instanceBindings.begin(),
                  instanceBindings.end());

  std::vector<VkVertexInputAttributeDescription> attributes{vertexAttributes};
  attributes.insert(attributes.end(), instanceAttributes.begin(),
                    instanceAttributes.end());

  // Put the bindings and attributes into the Vertex Input State
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size()),
      .pVertexBindingDescriptions = bindings.data(),
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attributes.size()),
      .pVertexAttributeDescriptions = attributes.data()};

  // -- Pipeline settings --
  VkPipelineRasterizationStateCreateInfo rasterizationState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .cullMode = CullBackFace ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  // Viewport & Scissor (Dynamic)
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};
  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  // Multisampling (Disabled)
  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  // Color Blending (Disabled)
  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  // Depths!
  VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp =
          VK_COMPARE_OP_LESS_OR_EQUAL // smallest depth should be displayed
  };

  VkPipelineRenderingCreateInfo pipelineRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &SwapchainImageFormat,
      .depthAttachmentFormat = engine.activeWindow->DepthImageFormat};

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &pipelineRenderingInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssemblyState,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizationState,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencilStateCreateInfo,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = layout};

  VkPipeline pipeline;
  checkVk(vkCreateGraphicsPipelines(engine.gpu, VK_NULL_HANDLE, 1,
                                    &pipelineInfo, nullptr, &pipeline),
          "Failed to create graphics pipeline!");

  return pipeline;
}

// This function will generate our graphics shader structure
inline GraphicsShader createGraphicsShader(
    Engine &engine, const char *shaderPath, uint32_t pushConstantSize = 0,
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT |
                                            VK_SHADER_STAGE_FRAGMENT_BIT,
    // Optional per-instance vertex binding/attributes we leave empty if the
    // shader has no instanced data.
    const std::vector<VkVertexInputBindingDescription> &instanceBindings = {},
    const std::vector<VkVertexInputAttributeDescription> &instanceAttributes =
        {}) {
  // Compile slang vertex/fragment shader to spir-v bytecode (and store it in a
  // module)
  VkShaderModule vertexModule = compileShader(engine, shaderPath, "VSMain");
  VkShaderModule fragmentModule = compileShader(engine, shaderPath, "FSMain");

  // Create pipeline, which holds the compiled code + various settings and
  // layout info
  VkPipelineLayout layout = createGraphicsPipelineLayout(
      engine, pushConstantSize, pushConstantStages);
  VkPipeline pipeline =
      createGraphicsPipeline(engine, layout, vertexModule, fragmentModule,
                             instanceBindings, instanceAttributes);

  vkDestroyShaderModule(engine.gpu, vertexModule, nullptr);
  vkDestroyShaderModule(engine.gpu, fragmentModule, nullptr);

  GraphicsShader shader{.pipelineLayout = layout,
                        .pipeline = pipeline,
                        .pushConstantStages =
                            pushConstantSize > 0 ? pushConstantStages : 0,
                        .pushConstantSize = pushConstantSize};

  return shader;
}

// Push function to validate the push constants before actually sending them off
inline void push(VkCommandBuffer cmdBuffer, const GraphicsShader &shader,
                 uint32_t size, const void *data) {
  if (data == nullptr || size == 0) {
    std::cerr << "ERROR: Invalid push constant data!\n";
    exit(EXIT_FAILURE);
  }

  if (size > shader.pushConstantSize) {
    std::cerr
        << "ERROR: Push constant data too big! Your shader only supports: "
        << shader.pushConstantSize << "! (This is in bytes!)\n";
    exit(EXIT_FAILURE);
  }

  vkCmdPushConstants(cmdBuffer, shader.pipelineLayout,
                     shader.pushConstantStages, 0, size, data);
}
