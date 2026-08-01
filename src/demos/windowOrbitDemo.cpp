#include "../../include/demos/windowOrbitDemo.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

namespace WindowOrbitDemo {
void run(Engine &engine) {
  std::cout << "Starting Planet & Moon window orbit demo...\n";

  // We assume main.cpp created exactly two windows before calling this.
  // Window 0 is the Planet, Window 1 is the Moon.
  Window *planet = &engine.windows[0];
  Window *moon = &engine.windows[1];

  float theta = 0.0f;
  const float orbitRadius = 350.0f; // Distance from the planet in pixels
  const float orbitSpeed = 0.02f;   // Speed of the orbit

  bool demoRunning = true;

  // Whilst the demo is running
  while (demoRunning) {
    glfwPollEvents();

    // If either window is closed, kill the loop
    if (glfwWindowShouldClose(planet->glfwWindow) ||
        glfwWindowShouldClose(moon->glfwWindow)) {
      demoRunning = false;
      break;
    }

    // Calculate the OS Window Orbit
    theta += orbitSpeed;

    // Get Planet position and size
    int pX, pY, pWidth, pHeight;
    glfwGetWindowPos(planet->glfwWindow, &pX, &pY);
    glfwGetWindowSize(planet->glfwWindow, &pWidth, &pHeight);

    // Get Moon size (so we can center it on the orbit path, not offset by its
    // top-left corner)
    int mWidth, mHeight;
    glfwGetWindowSize(moon->glfwWindow, &mWidth, &mHeight);

    // Find the exact center of the Planet window
    float centerX = pX + (pWidth / 2.0f) - (mWidth / 2.0f);
    float centerY = pY + (pHeight / 2.0f) - (mHeight / 2.0f);

    // Apply orbital math
    int moonX = static_cast<int>(centerX + orbitRadius * std::cos(theta));
    int moonY = static_cast<int>(centerY + orbitRadius * std::sin(theta));

    // Force the moon to its new orbital coordinate
    glfwSetWindowPos(moon->glfwWindow, moonX, moonY);

    // Render both windows
    for (auto &window : engine.windows) {
      beginFrame(engine, &window, glm::vec4(0.0, 0.0, 0.0, 1.0));

      // You could change the VkRenderingAttachmentInfo clearValue here
      // to make the planet blue and the moon grey!

      endFrame(engine, &window);
    }
  }

  // Ensure the GPU is finished rendering before the program exits and destroys
  // resources
  vkDeviceWaitIdle(engine.gpu);
}
} // namespace WindowOrbitDemo
