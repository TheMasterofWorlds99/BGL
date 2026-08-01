#include "../../include/demos/triangleDemo.hpp"
#include "../../include/engine.hpp"
#include "../../include/geometry.hpp"
#include "../../include/renderer.hpp"
#include "../../include/shaders.hpp"

namespace TriangleDemo {

void run(Engine &engine) {
  std::cout << "Starting Triangle Demo...\n";

  // Create our geometry!
  GPUMesh triangle = createTriangleTestMesh(engine.allocator);

  // Compile our slang shader, and ensure that the path is the path from where
  // your running the code!
  GraphicsShader shader =
      createGraphicsShader(engine, "shaders/triangleTest/triangle.slang");

  bool demoRunning = true;

  // The main render loop
  while (demoRunning) {
    glfwPollEvents();

    if (glfwWindowShouldClose(engine.activeWindow->glfwWindow)) {
      demoRunning = false;
      break;
    }

    // Prepare the frame and get the current command buffer
    beginFrame(engine, engine.activeWindow, glm::vec4(0.0, 0.0, 0.0, 1.0));

    // Grab the command buffer for this specific frame-in-flight
    VkCommandBuffer cmd =
        engine.activeWindow
            ->commandBuffers[engine.activeWindow->frameInFlightIndex];

    // Now we draw the mesh
    drawMesh(cmd, triangle, shader, 1);

    // Submit the commands to the GPU and present the screen
    endFrame(engine, engine.activeWindow);
  }

  // Wait for the GPU to finish its final frame before we delete the buffers
  vkDeviceWaitIdle(engine.gpu);

  // Destroy the Shader Pipeline
  vkDestroyPipeline(engine.gpu, shader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, shader.pipelineLayout, nullptr);

  // Destroy the Mesh Buffers (Free the VMA memory!)
  vmaDestroyBuffer(engine.allocator, triangle.vertexBuffer.vkBuffer,
                   triangle.vertexBuffer.alloc);
  vmaDestroyBuffer(engine.allocator, triangle.indexBuffer.vkBuffer,
                   triangle.indexBuffer.alloc);

  std::cout << "Exiting Triangle Demo...\n";
}

} // namespace TriangleDemo
