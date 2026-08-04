#pragma once

// Includes
#include "engine.hpp"
#include "memory.hpp"
#include <cstdint>
#include <vector>
#include <volk.h>

/*

   shaders.hpp will contain all the functions for the graphics
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

// How the pipeline blends fragments with what's already in the framebuffer.
// BlendMode::None is opaque (replace); Alpha and Additive both enable
// blending — the latter is great for glowing particles (overlaps add light
// instead of covering).
enum class BlendMode {
  None,
  Alpha,
  Additive,
};

// This function will allow us to compile the Slang into spir-v and put it in a
// VkShaderModule
VkShaderModule compileShader(const Engine &engine, const char *shaderPath,
                             const char *entryPointName);

// Create a descriptor set layout with a single storage buffer binding. This
// lets shaders read per-instance (or per-object) data uploaded from the CPU
VkDescriptorSetLayout createStorageBufferDescriptorSetLayout(
    const Engine &engine, VkShaderStageFlags stageFlags, uint32_t binding = 0);

// Create a descriptor pool big enough to allocate 'maxSets' sets, where each
// set holds one storage buffer descriptor
VkDescriptorPool createDescriptorPool(const Engine &engine, uint32_t maxSets);

// Allocate a single descriptor set out of the pool, matching the given layout
VkDescriptorSet allocateDescriptorSet(const Engine &engine,
                                      VkDescriptorPool pool,
                                      VkDescriptorSetLayout layout);

// Point the storage buffer descriptor in 'set' at a GPU buffer, so the shader
// can read its contents
void updateStorageBufferDescriptorSet(const Engine &engine, VkDescriptorSet set,
                                      const GPUBuffer &buffer);

// Create the Pipeline Layout (Empty for now, used later for pushing
// matrices/textures to GPU)
VkPipelineLayout createGraphicsPipelineLayout(
    const Engine &engine, uint32_t pushConstantSize = 0,
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT,
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE);

// This function will create the graphics pipeline!
VkPipeline createGraphicsPipeline(
    const Engine &engine, VkPipelineLayout layout, VkShaderModule vertexModule,
    VkShaderModule fragmentModule,
    // Optional second vertex binding for per-instance data (e.g. instance
    // position/rotation), stepped once per instance instead of once per
    // vertex. Left empty for shaders that don't use instancing.
    const std::vector<VkVertexInputBindingDescription> &instanceBindings = {},
    const std::vector<VkVertexInputAttributeDescription> &instanceAttributes =
        {},
    BlendMode blendMode = BlendMode::None);

// This function will generate our graphics shader structure
GraphicsShader createGraphicsShader(
    Engine &engine, const char *shaderPath, uint32_t pushConstantSize = 0,
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT |
                                            VK_SHADER_STAGE_FRAGMENT_BIT,
    // Optional per-instance vertex binding/attributes we leave empty if the
    // shader has no instanced data.
    const std::vector<VkVertexInputBindingDescription> &instanceBindings = {},
    const std::vector<VkVertexInputAttributeDescription> &instanceAttributes =
        {},
    BlendMode blendMode = BlendMode::None);

// Push function to validate the push constants before actually sending them off
void push(VkCommandBuffer cmdBuffer, const GraphicsShader &shader,
          uint32_t size, const void *data);
