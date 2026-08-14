#include "../../include/demos/galleryDemo.hpp"
#include "../../include/BGL.hpp"
#include "engine.hpp"
#include "geometry.hpp"
#include "imgui.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <vulkan/vulkan_core.h>

namespace GalleryDemo {

struct PushConstants {
  glm::mat4 viewProj;
};

struct GalleryEntry {
  std::string name;
  std::string description;
  GPUMesh mesh;
};

void run(Engine &engine) {
  std::cout << "Starting Gallery Demo...\n";

  // Grab our models!
  std::vector<GalleryEntry> gallery;
  gallery.reserve(3);

  gallery.push_back(GalleryEntry{
      .name = "Stanford Bunny",
      .description =
          "The Stanford bunny is a 3D test model developed in 1994 by Greg "
          "Turk and Marc Levoy at Stanford University. It was created by "
          "scanning a ceramic bunny figurine with a laser range finder, "
          "making it one of the most widely used models in computer graphics "
          "research — appearing everywhere from ray-tracing papers to "
          "rendering benchmarks.",
      .mesh = loadMeshFromObj(engine.allocator, "models/demo/bunny.obj")});

  gallery.push_back(GalleryEntry{
      .name = "Suzanne",
      .description =
          "Suzanne is the classic test model of Blender, a low-polygon monkey "
          "head created by Willem-Paul van Overbruggen (alias 'Sundial'). "
          "Named after a character from Fear and Loathing in Las Vegas, "
          "Suzanne is Blender's answer to the Stanford bunny, and a beloved "
          "mascot in the 3D community.",
      .mesh = loadMeshFromObj(engine.allocator, "models/demo/suzane.obj")});

  gallery.push_back(GalleryEntry{
      .name = "Chinese Dragon",
      .description =
          "A high-resolution laser scan of a ceramic Chinese dragon figurine "
          "from the Stanford 3D Scanning Repository - roughly 870,000 "
          "triangles of dense scan data. One of the classic stress-test "
          "models for high-polygon rendering.",
      .mesh = loadMeshFromObj(engine.allocator, "models/demo/Big_dragon.obj")});

  // Allows for the choosing between models
  size_t currentModel = 0;

  std::cout << "Loaded Meshs!\n";

  GraphicsShader Gshader = createGraphicsShader(
      engine, "shaders/galleryDemo/shader.slang", sizeof(PushConstants));

  bool demoRunning = true;
  float WindowWidth = windowWidth(engine.activeWindow);
  float WindowHeight = windowHeight(engine.activeWindow);

  // Camera stats
  glm::vec3 target = glm::vec3(0.0f);
  float radius = 2.5f;
  Camera camera;

  // Input struct setup
  Input input;

  while (demoRunning) {
    tickTimer(engine);
    glfwPollEvents();
    input.update(engine.activeWindow->glfwWindow);

    imguiBeginFrame();

    if (isInputDownThisFrame(engine, GLFW_KEY_ESCAPE)) {
      std::cout << "Escaped Pressed - closing window...\n";
      glfwSetWindowShouldClose(engine.activeWindow->glfwWindow, true);
    }

    if (glfwWindowShouldClose(engine.activeWindow->glfwWindow)) {
      demoRunning = false;
      break;
    }

    // Inputs!
    if (!ImGui::GetIO().WantCaptureMouse &&
        input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
      camera.yaw += input.mouseDeltaX * 0.008f;
      camera.pitch -= input.mouseDeltaY * 0.008f;
      // Pole guarding
      camera.pitch =
          glm::clamp(camera.pitch, glm::radians(-89.0f), glm::radians(89.0f));
    }

    // Scroll to zoom
    radius -= input.scrollDelta * 0.5f;
    radius = glm::clamp(radius, 1.5f, 50.0f);

    // Make sure the camera moves in an orbit around the center
    camera.position = target - cameraForward(camera) * radius;

    // Matrices
    glm::mat4 view = glm::lookAt(camera.position, target, cameraUp(camera));
    glm::mat4 proj = glm::perspectiveRH_ZO(
        glm::radians(60.0f),
        static_cast<float>(windowWidth(engine.activeWindow)) /
            static_cast<float>(windowHeight(engine.activeWindow)),
        0.01f, 100.0f);

    proj[1][1] *= -1.0f; // Flip to fit vulkan Y dir
    glm::mat4 viewProj = proj * view;

    WindowWidth = float(windowWidth(engine.activeWindow));
    WindowHeight = float(windowHeight(engine.activeWindow));

    // UI: floating swap buttons, bottom-center, no window chrome. The window
    // is invisible (NoBackground/NoDecoration) — only the buttons show.
    ImGuiIO &io = ImGui::GetIO();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    // Anchor the window's center-bottom at the middle of the screen
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 60.0f),
        ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::Begin("##SwapButtons", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::ArrowButton("##left", ImGuiDir_Left))
      currentModel = (currentModel - 1 + gallery.size()) % gallery.size();
    ImGui::SameLine();
    if (ImGui::ArrowButton("##right", ImGuiDir_Right))
      currentModel = (currentModel + 1) % gallery.size();

    ImGui::Text("Model %zu / %zu", currentModel + 1, gallery.size());

    ImGui::End();
    ImGui::PopStyleVar(2);

    // Info panel: current model's name (large) + description, top-left
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("##Info", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::SetWindowFontScale(1.5f); // name, large
    ImGui::TextUnformatted(gallery[currentModel].name.c_str());

    ImGui::SetWindowFontScale(1.0f); // description, normal
    ImGui::PushTextWrapPos(340.0f);
    ImGui::TextWrapped(gallery[currentModel].description.c_str());
    ImGui::PopTextWrapPos();

    ImGui::End();

    // Corner UI: stats pinned to the top-right
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGui::Begin("Stats", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);

    static float displayFPS = 0.0f;
    static float displayFrameTime = 0.0f;
    static float timer = 0.0f;

    float dt = engine.globalTimer.deltaTime;
    timer += dt;

    if (timer >= 0.25f) { // Update 4 times/sec
      displayFPS = (dt > 0.0f) ? 1.0f / dt : 0.0f;
      displayFrameTime = dt * 1000.0f;
      timer = 0.0f;
    }

    ImGui::Text("FPS: %.1f", displayFPS);
    ImGui::Text("Frame: %.3f ms", displayFrameTime);
    ImGui::Text("Tick: %llu",
                static_cast<unsigned long long>(engine.globalTimer.tickCount));

    ImGui::Separator();

    ImGui::Text("Resolution: %ux%u", engine.activeWindow->swapchainExtent.width,
                engine.activeWindow->swapchainExtent.height);

    ImGui::End();

    beginFrame(engine, engine.activeWindow, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));

    VkCommandBuffer cmd =
        engine.activeWindow
            ->commandBuffers[engine.activeWindow->frameInFlightIndex];

    PushConstants pc{.viewProj = viewProj};

    // Draw the current gallery model with the view-proj pushed
    drawMesh(cmd, gallery[currentModel].mesh, Gshader, 1, &pc, sizeof(pc));

    imguiRender(cmd);

    endFrame(engine, engine.activeWindow);
  }

  // Wait for the GPU to finish before tearing anything down
  vkDeviceWaitIdle(engine.gpu);

  // Destroy the shader pipeline
  vkDestroyPipeline(engine.gpu, Gshader.pipeline, nullptr);
  vkDestroyPipelineLayout(engine.gpu, Gshader.pipelineLayout, nullptr);

  // Free the mesh buffers for every gallery model
  for (auto &entry : gallery) {
    vmaDestroyBuffer(engine.allocator, entry.mesh.vertexBuffer.vkBuffer,
                     entry.mesh.vertexBuffer.alloc);
    vmaDestroyBuffer(engine.allocator, entry.mesh.indexBuffer.vkBuffer,
                     entry.mesh.indexBuffer.alloc);
  }

  std::cout << "Exiting 4D Demo...\n";
}

} // namespace GalleryDemo
