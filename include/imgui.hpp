#pragma once

#include "engine.hpp"

void initImgui(Engine &engine, Window &window);

void imguiBeginFrame();

void imguiRender(VkCommandBuffer cmdBuffer);

void cleanupImgui();
