// Including the library is a single header. The demo's own header pulls in
// run() — everything else (Engine, initEngine, cleanupEngine) comes from BGL1.
#include "../../include/BGL1.hpp"
#include "../../include/demos/3DDemo.hpp"
#include <iostream>

// Standalone main for the 3D (bunnies) demo. Build & run this demo alone:
//   cmake --build build && ./build/vulkanEngine
int main() {
  std::cout << "3D test!\n";

  Engine engine;
  initEngine(engine, "Breath taking 3D visuals", 1280, 720);

  Demo3D::run(engine);

  std::cout << "Cleaning up engine resources...\n";
  cleanupEngine(engine);

  return 0;
}
