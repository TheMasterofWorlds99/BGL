#pragma once

// Includes
#include "engine.hpp"
#include <cstdint>

/*

   queues.hpp handles all the code for family queues and commands which will
   allow us to send well, commands to the GPU so we can draw stuff

*/

struct QueueFamilyIndices {
  uint32_t graphicsFamilyIndex = 0;
  bool foundGraphics = false;
};

// The GPU has a bunch of queue families that we want to look through. The goal
// is to find one that allows us to send graphics commands through it. This
// function simply takes the physical GPU, and searchs through it's queue
// families to find it
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

// This is a function to create command buffers which we'll send to the GPU to
// draw stuff
void createCommandBuffers(const Engine &engine, Window &window);
