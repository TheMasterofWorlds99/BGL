#include "../../include/BGL1.hpp"
#include "../../include/demos/triangleDemo.hpp"
#include <iostream>

// Standalone main for the triangle demo. Build & run:
//   cmake --build build && ./build/triangleDemo
int main() {
  Engine engine;
  initEngine(engine, "Triangle Demo", 1280, 720);

  TriangleDemo::run(engine);

  cleanupEngine(engine);
  return 0;
}
