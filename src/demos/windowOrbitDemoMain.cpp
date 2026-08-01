#include "../../include/BGL1.hpp"
#include "../../include/demos/windowOrbitDemo.hpp"
#include <iostream>

// Standalone main for the window orbit demo (two windows). Build & run:
//   cmake --build build && ./build/windowOrbitDemo
int main() {
  Engine engine;
  initEngine(engine, "Window Orbit Demo", 1280, 720, 2);

  WindowOrbitDemo::run(engine);

  cleanupEngine(engine);
  return 0;
}
