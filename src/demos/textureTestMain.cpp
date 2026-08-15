#include "../../include/BGL.hpp"
#include "../../include/demos/textureTest.hpp"
#include <iostream>

// Standalone main for the texture test demo. Build & run:
//   cmake --build build && ./build/textureTest
int main() {
  Engine engine;
  initEngine(engine, "Texture Test", 800, 600);

  TextureTest::run(engine);

  cleanupEngine(engine);
  return 0;
}
