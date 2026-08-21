#include "../../include/BGL.hpp"
#include "../../include/demos/glbTest.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

namespace GlbTest {

struct PushConstants {
  glm::mat4 viewProj;
};

void run(Engine &engine) {
  std::cout << "Starting GLB Test...\n";

  // Load the glb with its base-color textures
  std::vector<GlbPrimitive> prims =
      loadGlb(engine, "models/demo/KatasL0Wall1.glb");
  if (prims.empty()) {
    std::cerr << "No primitives loaded\n";
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < prims.size(); i++)
    std::cout << "prim " << i << ": " << prims[i].mesh.vertices.size()
              << " verts, " << prims[i].mesh.indices.size() / 3 << " tris"
              << (prims[i].hasTexture ? " [textured]" : " [no texture]")
              << "\n";

  // Descriptor plumbing: one layout, one set per primitive
  VkDescriptorSetLayout texLayout = createSampledImageDescriptorSetLayout(
      engine, VK_SHADER_STAGE_FRAGMENT_BIT);
  VkDescriptorPool pool = createDescriptorPool(
      engine, static_cast<uint32_t>(prims.size() + 1),
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  // A 1x1 white fallback so texture-less primitives still sample something
  uint8_t white[4] = {255, 255, 255, 255};
  Texture whiteTex = createTexture(engine, 1, 1, white);

  std::vector<VkDescriptorSet> sets;
  for (auto &p : prims) {
    VkDescriptorSet set = allocateDescriptorSet(engine, pool, texLayout);
    updateSampledImageDescriptorSet(engine, set, p.hasTexture ? p.baseColor
                                                              : whiteTex);
    sets.push_back(set);
  }

  GraphicsShader shader = createGraphicsShader(
      engine, "shaders/glbTest/shader.slang", sizeof(PushConstants),
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {}, {},
      BlendMode::None, texLayout);

  // Fit the orbit camera to the model's bounds
  glm::vec3 minV(1e9f), maxV(-1e9f);
  for (const auto &p : prims)
    for (const auto &v : p.mesh.vertices) {
      minV = glm::min(minV, v.pos);
      maxV = glm::max(maxV, v.pos);
    }
  glm::vec3 target = (minV + maxV) * 0.5f;
  float radius = glm::length(maxV - minV) * 0.75f + 1.0f;

  Camera camera;
  Input input;
  bool demoRunning = true;

  while (demoRunning) {
    tickTimer(engine);
    glfwPollEvents();
    input.update(engine.activeWindow->glfwWindow);

    if (isInputDownThisFrame(engine, GLFW_KEY_ESCAPE))
      glfwSetWindowShouldClose(engine.activeWindow->glfwWindow, true);
    if (glfwWindowShouldClose(engine.activeWindow->glfwWindow)) {
      demoRunning = false;
      break;
    }

    // Orbit: drag to rotate, scroll to zoom
    if (input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
      camera.yaw -= input.mouseDeltaX * 0.008f;
      camera.pitch -= input.mouseDeltaY * 0.008f;
      camera.pitch =
          glm::clamp(camera.pitch, glm::radians(-89.0f), glm::radians(89.0f));
    }
    radius -= input.scrollDelta * 0.5f;
    radius = glm::clamp(radius, 0.5f, 200.0f);
    camera.position = target - cameraForward(camera) * radius;

    glm::mat4 view =
        glm::lookAt(camera.position, target, cameraUp(camera));
    glm::mat4 proj = glm::perspectiveRH_ZO(
        glm::radians(60.0f),
        static_cast<float>(windowWidth(engine.activeWindow)) /
            static_cast<float>(windowHeight(engine.activeWindow)),
        0.01f, 500.0f);
    PushConstants pc{.viewProj = proj * view};

    beginFrame(engine, engine.activeWindow,
               glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

    VkCommandBuffer cmd =
        engine.activeWindow
            ->commandBuffers[engine.activeWindow->frameInFlightIndex];

    for (size_t i = 0; i < prims.size(); i++)
      drawMesh(cmd, prims[i].mesh, shader, 1, &pc, sizeof(pc), nullptr,
               sets[i]);

    endFrame(engine, engine.activeWindow);
  }

  vkDeviceWaitIdle(engine.gpu);
  vkDestroyPipeline(engine.gpu, shader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, shader.pipelineLayout, nullptr);
  vkDestroyDescriptorPool(engine.gpu, pool, nullptr);
  vkDestroyDescriptorSetLayout(engine.gpu, texLayout, nullptr);
  destroyTexture(engine, whiteTex);
  for (auto &p : prims) {
    vmaDestroyBuffer(engine.allocator, p.mesh.vertexBuffer.vkBuffer,
                     p.mesh.vertexBuffer.alloc);
    vmaDestroyBuffer(engine.allocator, p.mesh.indexBuffer.vkBuffer,
                     p.mesh.indexBuffer.alloc);
    if (p.hasTexture)
      destroyTexture(engine, p.baseColor);
  }

  std::cout << "Exiting GLB Test...\n";
}

} // namespace GlbTest
