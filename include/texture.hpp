#pragma once

#include "engine.hpp"
#include <cstdint>

struct Texture {
  VkImage image;
  VmaAllocation alloc;
  VkImageView view;
  VkSampler sampler;
  uint32_t width;
  uint32_t height;
};

Texture createTexture(Engine &engine, uint32_t width, uint32_t height,
                      const uint8_t *pixels);

void destroyTexture(Engine &engine, Texture &texture);
