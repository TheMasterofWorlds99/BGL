#include "../../include/BGL.hpp"
#include "../../include/demos/galleryDemo.hpp"
#include <iostream>

// Standalone main for the 3D gallery demo. Build & run:
//   cmake --build build && ./build/galleryDemo
int main() {
  Engine engine;
  initEngine(engine, "BGL 3D Gallery", 1280, 720);
  initImgui(engine, engine.windows[0]);

  GalleryDemo::run(engine);

  cleanupImgui();
  cleanupEngine(engine);
  return 0;
}
