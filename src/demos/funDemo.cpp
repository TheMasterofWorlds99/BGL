#include "../../include/demos/funDemo.hpp"
#include "../../include/engine.hpp"
#include "../../include/geometry.hpp"
#include "../../include/renderer.hpp"
#include "../../include/shaders.hpp"
#include <iostream>

// Must match the PushConstants struct declared in the shader file so we can
// actually properly send the important data to it
struct FunDemoPushConstants {
  float fov;        // Vertical field of view, in degrees
  float aspect;     // Window width / height
  float pitch;      // Camera pitch, in degrees
  float viewHeight; // Camera height above the ground plane
};

namespace FunDemo {

void run(Engine &engine) {
  std::cout << "Starting Fun Demo...\n";

  // Create our geometry!
  GPUMesh triangle = createTriangleTestMesh(engine.allocator);

  // Compile our slang shader, and ensure that the path is the path from where
  // your running the code!
  GraphicsShader shader = createGraphicsShader(
      engine, "shaders/funStuff/shader.slang", sizeof(FunDemoPushConstants));

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

    // Fill out the parameters the shader is going to need this frame
    // We recompute aspect ratio every frame to ensure that it works correctly,
    // even if the window is resized
    FunDemoPushConstants pushConstants{
        .fov = 60.0f,
        .aspect = static_cast<float>(windowWidth(engine.activeWindow)) /
                  static_cast<float>(windowHeight(engine.activeWindow)),
        .pitch = 12.0f,
        .viewHeight = 4.0f,
    };

    // Now we draw the mesh
    drawMesh(cmd, triangle, shader, 1, &pushConstants, sizeof(pushConstants));

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

  std::cout << "Exiting Fun Demo...\n";
}

} // namespace FunDemo
