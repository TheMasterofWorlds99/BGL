#pragma once

// Includes
#include "geometry.hpp"
#include "shaders.hpp"
#include <cstdint>
#include <volk.h>

/*

   renderer.hpp is the file that will contain all the rendering functions and
   drawing for the library.

*/

void drawMesh(VkCommandBuffer cmdBuffer, const GPUMesh &mesh,
              const GraphicsShader &shader, uint32_t instanceCount,
              const void *pushData = nullptr, uint32_t pushSize = 0,
              // Optional per-instance vertex buffer. Just
              // a plain buffer the CPU writes instance data into every
              // frame
              const GPUBuffer *instanceBuffer = nullptr);

// -- COMPUTE!!! --

// Struct mirroring your requested design
struct SlangReflectionInfo {
  uint32_t computeThreadGroupSizeX = 64;
};

struct ComputeKernel {
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
  SlangReflectionInfo reflectionInfo;
  uint32_t pushConstantSize = 0;
};

// Simple barrier to ensure compute finishes before graphics reads the vertex
// buffer
void cmdComputeMemoryBarrier(VkCommandBuffer cmdBuffer);

// Exactly as requested in your image!
void runCompute1D(VkCommandBuffer cmdBuffer, const ComputeKernel &compute,
                  uint32_t threadCount, const void *pushData = nullptr,
                  uint32_t pushSize = 0);

// Now we create the functions to handle the create compute pipeline!
VkPipelineLayout createComputePipelineLayout(const Engine &engine,
                                             uint32_t pushConstantSize = 0);

VkPipeline createComputePipeline(const Engine &engine, VkPipelineLayout layout,
                                 VkShaderModule computeModule);

ComputeKernel createComputeKernel(const Engine &engine, const char *shaderPath,
                                  uint32_t pushConstantSize = 0,
                                  const char *entryPointName = "CSMain");
