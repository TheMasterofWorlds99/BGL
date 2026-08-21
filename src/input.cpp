#include "../include/input.hpp"
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include <cstdint>

InputCode::InputCode(uint32_t v) : value(v) {}
InputCode::operator uint32_t() const { return value; }

// GLFW has no way to *poll* scroll, so a single callback accumulates it until
// the next update() drains it. Shared by all windows for now.
static double pendingScroll = 0.0;

void scrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
  // Only forward to ImGui when it's actually initialized (demos without
  // ImGui would otherwise crash on uninitialized backend data)
  if (ImGui::GetCurrentContext())
    ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);
  (void)window;
  (void)xOffset;
  pendingScroll += yOffset;
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  (void)mods;
  if (ImGui::GetCurrentContext())
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

void Input::update(GLFWwindow *window) {
  // Cursor position + delta (polled; no event exists for it)
  double newX, newY;
  glfwGetCursorPos(window, &newX, &newY);
  mouseDeltaX = newX - mouseX;
  mouseDeltaY = newY - mouseY;
  mouseX = newX;
  mouseY = newY;

  // Mouse buttons (polled; true while held)
  for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; button++)
    mouseButtons[button] = glfwGetMouseButton(window, button) == GLFW_PRESS;

  // Drain the scroll accumulated since the last update
  scrollDelta = pendingScroll;
  pendingScroll = 0.0;
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  // Only forward to ImGui when it's actually initialized
  if (ImGui::GetCurrentContext())
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
  (void)scancode;
  (void)mods;
  Engine *engine = static_cast<Engine *>(glfwGetWindowUserPointer(window));
  uint32_t frame = engine->globalTimer.tickCount;

  // If key is unknown or ourside of the standard range, we do an early return;
  if (key < 0)
    return;
  if (key > GLFW_KEY_LAST)
    return;

  // Update up/down frame index for current key
  if (action == GLFW_PRESS) {
    engine->inputStates[key].keyDownFrame = frame;
  } else if (action == GLFW_RELEASE) {
    engine->inputStates[key].keyUpFrame = frame;
  }
}

bool isInputDownThisFrame(const Engine &engine, InputCode inputCode) {
  uint32_t inputIndex = static_cast<uint32_t>(inputCode);
  uint32_t frame = engine.globalTimer.tickCount;
  uint32_t keyDownFrame = engine.inputStates[inputIndex].keyDownFrame;
  return keyDownFrame == frame;
}

bool isInputHeld(const Engine &engine, InputCode inputCode) {
  uint32_t inputIndex = static_cast<uint32_t>(inputCode);
  const KeyState &state = engine.inputStates[inputIndex];
  // Most recent event was a press → currently held
  return state.keyDownFrame > state.keyUpFrame;
}
