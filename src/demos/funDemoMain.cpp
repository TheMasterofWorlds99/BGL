#include "../../include/BGL.hpp"
#include "../../include/demos/funDemo.hpp"
#include <iostream>

// Standalone main for the fun demo. Build & run:
//   cmake --build build && ./build/funDemo
int main() {
  Engine engine;
  initEngine(engine, "Fun Demo", 1280, 720);

  FunDemo::run(engine);

  cleanupEngine(engine);
  return 0;
}
