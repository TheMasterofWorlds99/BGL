#include "../../include/BGL.hpp"
#include "../../include/demos/textureTest.hpp"
#include <iostream>
#include <vector>

namespace TextureTest {

// A unit quad centered at the origin with UVs in [0,1]
GPUMesh createQuadMesh(VmaAllocator allocator) {
  std::vector<Vertex> vertices = {
      Vertex{.pos = {-0.5f, -0.5f, 0}, .normal = {0, 0, 1}, .uv = {0, 0}},
      Vertex{.pos = {0.5f, -0.5f, 0}, .normal = {0, 0, 1}, .uv = {1, 0}},
      Vertex{.pos = {0.5f, 0.5f, 0}, .normal = {0, 0, 1}, .uv = {1, 1}},
      Vertex{.pos = {-0.5f, 0.5f, 0}, .normal = {0, 0, 1}, .uv = {0, 1}},
  };
  // Reversed winding: the triangle mesh renders (CW-in-NDC → front in
  // Vulkan's y-down framebuffer), so the quad must match that convention.
  std::vector<uint32_t> indices = {0, 2, 1, 0, 3, 2};
  return createGPUMesh(allocator, vertices, indices);
}

void run(Engine &engine) {
  std::cout << "Starting Texture Test...\n";

  // 1. Generate a 16x16 checkerboard (8px cells)
  const uint32_t size = 16;
  std::vector<uint8_t> pixels(size * size * 4);
  for (uint32_t y = 0; y < size; y++) {
    for (uint32_t x = 0; x < size; x++) {
      bool on = ((x / 8) + (y / 8)) % 2 == 0;
      uint8_t v = on ? 255 : 0;
      size_t i = (y * size + x) * 4;
      pixels[i + 0] = v;
      pixels[i + 1] = v;
      pixels[i + 2] = v;
      pixels[i + 3] = 255;
    }
  }

  // 2. Texture + descriptor plumbing
  Texture tex = createTexture(engine, size, size, pixels.data());
  VkDescriptorSetLayout texLayout = createSampledImageDescriptorSetLayout(
      engine, VK_SHADER_STAGE_FRAGMENT_BIT);
  VkDescriptorPool pool =
      createDescriptorPool(engine, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  VkDescriptorSet texSet = allocateDescriptorSet(engine, pool, texLayout);
  updateSampledImageDescriptorSet(engine, texSet, tex);

  // 3. Quad + shader (with the texture descriptor layout)
  GPUMesh quad = createQuadMesh(engine.allocator);
  GraphicsShader shader = createGraphicsShader(
      engine, "shaders/textureTest/shader.slang", 0,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {}, {},
      BlendMode::None, texLayout);

  bool demoRunning = true;
  while (demoRunning) {
    tickTimer(engine);
    glfwPollEvents();

    if (isInputDownThisFrame(engine, GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(engine.activeWindow->glfwWindow, true);
    }
    if (glfwWindowShouldClose(engine.activeWindow->glfwWindow)) {
      demoRunning = false;
      break;
    }

    beginFrame(engine, engine.activeWindow,
               glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));

    VkCommandBuffer cmd =
        engine.activeWindow
            ->commandBuffers[engine.activeWindow->frameInFlightIndex];

    drawMesh(cmd, quad, shader, 1, nullptr, 0, nullptr, texSet);

    endFrame(engine, engine.activeWindow);
  }

  vkDeviceWaitIdle(engine.gpu);
  vkDestroyPipeline(engine.gpu, shader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, shader.pipelineLayout, nullptr);
  vkDestroyDescriptorPool(engine.gpu, pool, nullptr);
  vkDestroyDescriptorSetLayout(engine.gpu, texLayout, nullptr);
  destroyTexture(engine, tex);
  vmaDestroyBuffer(engine.allocator, quad.vertexBuffer.vkBuffer,
                   quad.vertexBuffer.alloc);
  vmaDestroyBuffer(engine.allocator, quad.indexBuffer.vkBuffer,
                   quad.indexBuffer.alloc);

  std::cout << "Exiting Texture Test...\n";
}

} // namespace TextureTest
