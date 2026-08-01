#include "../../include/demos/3DDemo.hpp"
#include "../../include/engine.hpp"
#include "../../include/geometry.hpp"
#include "../../include/input.hpp"
#include "../../include/renderer.hpp"
#include "../../include/shaders.hpp"
#include <iostream>
#include <random>

namespace Demo3D {

constexpr uint32_t InstanceCount = 10000;

// Must match the PushConstants struct declared in the shader, member for
// member, so the offsets line up (resolution@0, time@8, instancesPtr@16,
// deltaTime@24, count@28).
struct PushConstants {
  glm::vec2 resolution;
  float time;
  uint64_t instancesPtr;
  float deltaTime;
  int count;
};

struct RNG {
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist{0.0f, 1.0f};

  RNG(uint32_t seed) : gen(seed) {}
  float unorm() { return dist(gen); }
  float snorm() { return unorm() * 2.0f - 1.0f; }

  glm::vec3 direction() {
    while (true) {
      glm::vec3 p(snorm(), snorm(), snorm());
      float len2 = glm::dot(p, p);
      if (len2 > 0.0f && len2 <= 1.0f)
        return glm::normalize(p);
    }
  }

  glm::vec3 PointInUnitSphere() {
    while (true) {
      glm::vec3 p(snorm(), snorm(), snorm());
      if (glm::dot(p, p) <= 1.0f)
        return p;
    }
  }
};

// Sent to the GPU. Added 'padding' to ensure it is exactly 64 bytes total
// (16 + 16 + 16 + 4 + 4 + 4 + 4) which makes GPU array indexing lightning fast
// and perfectly aligned.
struct alignas(16) InstanceData {
  glm::vec4 pos;
  glm::vec4 vel;
  glm::vec4 spinAxis;
  float radius;
  float mass;
  float spinSpeed;
  float padding;
};

constexpr uint32_t InstancesSizeBytes = InstanceCount * sizeof(InstanceData);

// Line constants — keep in sync with the compute shader
// (shaders/3DDemo/shader.slang)!
constexpr float LineLength = 350.0f; // how far the cloud extends in -z
constexpr float LineSpread = 150.0f; // random x/y scatter around the path
constexpr float MinRadius = 10.0f;   // keep the camera path clear

// Generates the initial spawn state for the bunnies. We only run this ONCE now!
std::vector<InstanceData> initBunnyInstances() {
  std::vector<InstanceData> instances;
  instances.reserve(InstanceCount);

  constexpr int seed = 142;
  constexpr float sizeMin = 0.25f;
  constexpr float sizeMax = 1.75f;

  // Bunnies are scattered randomly in a long cloud along -z: x/y random
  // within a box around the flight path (camera path kept clear), z spread
  // along the line. They drift toward the camera and wrap around behind it
  // (compute shader), so this just seeds the cloud.
  RNG rng(seed);

  for (int i = 0; i < InstanceCount; i++) {
    float size = sizeMin + sizeMax * rng.unorm();
    float mass = size * size * size;

    float x;
    float y;
    do {
      x = (rng.unorm() - 0.5f) * 2.0f * LineSpread;
      y = (rng.unorm() - 0.5f) * 2.0f * LineSpread;
    } while (x * x + y * y < MinRadius * MinRadius);

    InstanceData instance{
        .pos = glm::vec4(x, y, -rng.unorm() * LineLength, 0),
        .vel = glm::vec4(0.0f), // unused now; bunnies just drift down the line
        .spinAxis = glm::vec4(rng.direction(), 0),
        .radius = size,
        .mass = mass,
        .spinSpeed = 0.5f + rng.unorm() * 3.5f, // rad/s, always forward
        .padding = 0.0f};
    instances.push_back(instance);
  }
  return instances;
}

void run(Engine &engine) {
  std::cout << "Starting 3D Demo...\n";

  GPUMesh model =
      loadMeshFromObj(engine.allocator, "models/demo/bunny_simple.obj");

  std::cout << "Loaded bunny mesh!\n";
  std::cout << "Vertices: " << model.vertices.size() << '\n';
  std::cout << "Indices: " << model.indices.size() << '\n';

  // 1. Generate CPU-side initial state
  auto initialInstances = initBunnyInstances();

  // 2. Create ONE massive GPU buffer that handles both Vertices AND Storage
  // (Compute) AND exposes its Memory Address (BDA)
  GPUBuffer instanceBuffer = createGPUBufferWithData(
      engine.allocator,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, // Required for GPU
                                                     // pointers!
      initialInstances.data(), InstancesSizeBytes);

  // 3. Extract the raw 64-bit GPU pointer!
  uint64_t instanceGPUAddress =
      getGPUBufferAddress(engine.gpu, instanceBuffer.vkBuffer);

  // Describe the per-instance vertex data (matches VSInput in shader)
  VkVertexInputBindingDescription instanceBinding{
      .binding = 1,
      .stride = sizeof(InstanceData),
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE};

  std::vector<VkVertexInputAttributeDescription> instanceAttributes{
      {.location = 3,
       .binding = 1,
       .format = VK_FLOAT4,
       .offset = offsetof(InstanceData, pos)},
      {.location = 4,
       .binding = 1,
       .format = VK_FLOAT4,
       .offset = offsetof(InstanceData, spinAxis)},
      {.location = 5,
       .binding = 1,
       .format = VK_FLOAT1,
       .offset = offsetof(InstanceData, radius)},
      {.location = 6,
       .binding = 1,
       .format = VK_FLOAT1,
       .offset = offsetof(InstanceData, spinSpeed)},
  };

  // Compile Graphics and Compute Shader

  GraphicsShader graphicsShader = createGraphicsShader(
      engine, "shaders/3DDemo/shader.slang", sizeof(PushConstants),
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      {instanceBinding}, instanceAttributes);

  ComputeKernel computeShader = createComputeKernel(
      engine, "shaders/3DDemo/shader.slang", sizeof(PushConstants));

  bool demoRunning = true;
  float WindowWidth = windowWidth(engine.activeWindow);
  float WindowHeight = windowHeight(engine.activeWindow);

  float previousTime = static_cast<float>(glfwGetTime());

  while (demoRunning) {
    // A new frame begins: advance the timer BEFORE polling events, so key
    // events get stamped with this frame's number (the ordering rule).
    tickTimer(engine);
    glfwPollEvents();

    // Smoke test for the input system: Esc closes the window. The key
    // callback stamps inputStates[ESC] with the current frame during
    // glfwPollEvents(), and this query matches it — for exactly one frame.
    if (isInputDownThisFrame(engine, GLFW_KEY_ESCAPE)) {
      std::cout << "Escape pressed - closing window\n";
      glfwSetWindowShouldClose(engine.activeWindow->glfwWindow, true);
    }

    if (glfwWindowShouldClose(engine.activeWindow->glfwWindow)) {
      demoRunning = false;
      break;
    }

    // Calculate delta time for the compute physics
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - previousTime;
    previousTime = currentTime;

    WindowWidth = float(windowWidth(engine.activeWindow));
    WindowHeight = float(windowHeight(engine.activeWindow));

    // Acquire the swapchain image and reset/begin the command buffer WITHOUT
    // starting the render pass — the compute pass must record first.
    VkCommandBuffer cmdBuffer =
        beginFrameCommandBuffer(engine, engine.activeWindow);

    PushConstants pushData{.resolution = vec2(WindowWidth, WindowHeight),
                           .time = currentTime,
                           .instancesPtr = instanceGPUAddress,
                           .deltaTime = deltaTime,
                           .count = InstanceCount};

    // Look how clean this is! No descriptors!
    runCompute1D(cmdBuffer, computeShader, InstanceCount, &pushData,
                 sizeof(pushData));

    // Now we manually begin the render pass!
    beginRenderPass(cmdBuffer, engine.activeWindow,
                    glm::vec4(0.0, 0.0, 0.0, 1.0));

    VkViewport viewport{.x = 0,
                        .y = WindowHeight,
                        .width = WindowWidth,
                        .height = -WindowHeight,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f};
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    // Draw the mesh using the SAME buffer the compute shader just finished
    // updating
    drawMesh(cmdBuffer, model, graphicsShader, InstanceCount, &pushData,
             sizeof(pushData), &instanceBuffer);

    endFrame(engine, engine.activeWindow);
  }

  // Wait for the GPU to finish before cleaning up
  vkDeviceWaitIdle(engine.gpu);

  // Clean up Compute
  vkDestroyPipeline(engine.gpu, computeShader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, computeShader.pipelineLayout, nullptr);

  // Clean up Graphics
  vkDestroyPipeline(engine.gpu, graphicsShader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, graphicsShader.pipelineLayout, nullptr);

  // Clean up the single shared instance buffer
  vmaDestroyBuffer(engine.allocator, instanceBuffer.vkBuffer,
                   instanceBuffer.alloc);

  // Clean up Mesh
  vmaDestroyBuffer(engine.allocator, model.vertexBuffer.vkBuffer,
                   model.vertexBuffer.alloc);
  vmaDestroyBuffer(engine.allocator, model.indexBuffer.vkBuffer,
                   model.indexBuffer.alloc);

  std::cout << "Exiting 3D Demo...\n";
}

} // namespace Demo3D
