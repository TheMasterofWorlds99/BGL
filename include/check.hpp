#pragma once

// Includes
#include <iostream>
#include <volk.h>

/*

   check.hpp holds all the check functions I use throughout the library, like
   checkVk, checkGLFW, and checkVolk.

*/

// General checkVk for checking all vulkan functions that return VkResult
inline void checkVk(VkResult vkResult, const char *errorMsg) {
  if (vkResult != VK_SUCCESS) {
    std::cerr << "ERROR: " << errorMsg << "\n";
    exit(EXIT_FAILURE);
  }
}

// Helper functions for checking the initalization of Glfw and Volk
inline void checkGlfw(int glfwInitCode) {
  // Since we only throw in glfwInit() for the input of this function, it can
  // only be 0 or 1, which is simply true or false. This is why the ! works on
  // it
  if (!glfwInitCode) {
    std::cerr << "ERROR: GLFW Failed to initalize!\n";
    exit(EXIT_FAILURE);
  }
}

inline void checkVolk(VkResult volkResult) {
  // VK_SUCCESS is exactly 0, so we can simply see if the result is != to that
  // and if it isn't, exit similarly to glfw as seen above
  if (volkResult != VK_SUCCESS) {
    std::cerr << "ERROR: Volk Failed to initalize!\n";
    exit(EXIT_FAILURE);
  }
}
