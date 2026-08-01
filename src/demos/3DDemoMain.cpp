#include "../../include/demos/3DDemo.hpp"
#include "../../include/vulkanInit/vkInit.hpp"
#include <iostream>

// Standalone main for the 3D (bunnies) demo. Build & run this demo alone:
//   cmake --build build && ./build/3DDemo
int main() {
  std::cout << "3D test!\n";

  Engine engine;
  initEngine(engine, "Breath taking 3D visuals", 1280, 720);

  Demo3D::run(engine);

  std::cout << "Cleaning up engine resources...\n";
  cleanupEngine(engine);

  return 0;
}
