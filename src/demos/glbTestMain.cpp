#include "../../include/BGL.hpp"
#include "../../include/demos/glbTest.hpp"
#include <iostream>

// Standalone main for the glb viewer test. Build & run:
//   cmake --build build && ./build/glbTest
int main() {
  Engine engine;
  initEngine(engine, "GLB Test", 1280, 720);

  GlbTest::run(engine);

  cleanupEngine(engine);
  return 0;
}
