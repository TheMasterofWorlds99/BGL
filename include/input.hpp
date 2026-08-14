#pragma once

// Includes
#include "engine.hpp"

/*

   input.hpp is the file that contains the input struct which will handle all
   keyboard and mouse inputs from the user for the library

*/

// A distinct wrapper around a GLFW input code (key or button) so the query
// API reads well at call sites and can grow (mouse, scroll) without breaking
// anything. Both conversions are implicit on purpose.
struct InputCode {
  uint32_t value;
  InputCode(uint32_t v);
  operator uint32_t() const;
};

// Per-frame mouse/cursor state. Keys live in the engine (inputStates, stamped
// by keyCallback); the mouse is split: cursor + buttons are polled here each
// frame, scroll arrives via a callback and is drained here.
struct Input {
  // Cursor position, in pixels, origin at the window's top-left (+y down)
  double mouseX = 0.0;
  double mouseY = 0.0;
  // Cursor movement since the last update
  double mouseDeltaX = 0.0;
  double mouseDeltaY = 0.0;
  // Scroll accumulated since the last update (positive = scroll up)
  double scrollDelta = 0.0;
  // Mouse buttons, true while held
  bool mouseButtons[GLFW_MOUSE_BUTTON_LAST + 1]{};

  // Refresh cursor + buttons from GLFW and drain the scroll accumulator.
  // Call once per frame, right after glfwPollEvents().
  void update(GLFWwindow *window);

  bool isMouseDown(int button) const { return mouseButtons[button]; }
};

// GLFW key callback: stamps the current frame onto the key's down/up state.
// Registered per-window during engine setup; reaches the Engine via the
// window's user pointer.
void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods);

// GLFW scroll callback: accumulates scroll into the input system (GLFW has no
// way to poll scroll, so this is the only path).
void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);

// GLFW mouse button callback: forwards to ImGui (BGL polls buttons itself in
// Input::update, so this exists purely to feed the UI).
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);

// True only during the exact frame the input was pressed.
bool isInputDownThisFrame(const Engine &engine, InputCode inputCode);

// True for every frame while the input is held (keyDownFrame > keyUpFrame).
bool isInputHeld(const Engine &engine, InputCode inputCode);
