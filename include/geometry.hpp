#pragma once

// Includes
#include "engine.hpp"

// Other Includes
#include "memory.hpp"
#include <cstdint>
#include <tiny_obj_loader.h>
#include <vector>

/*

   geometry.hpp will handle all the 3D meshes, geometry, and math that the
   graphics library will use. Need to draw a triangle? The mesh struct and
   glm math functions here will allow you to easily handle it.

   IMPORTANT!!!!
   Vulkan treats it's screen coordinates as follows:

   (-1, -1) --------------------------------------------- (+1, -1)
   |                                                             |
   |                                                             |
   |                                                             |
   |                                                             |
   |                                                             |
   |                                                             |
   |                                                             |
   |                                                             |
   (-1, +1) --------------------------------------------- (+1, +1)

*/

// We're using glm::vec3 and glm::vec2 to make writing down vectors very easy.
// NOTE: glm vectors are floats unless written as glm::dvecx where x is the size
// of the vector (2, 3 or 4)
using glm::vec2;
using glm::vec3;

struct Vertex {
  vec3 pos;    // The postion of the vector
  vec3 normal; // The vectors normal (the direction it faces outward from the
               // object it's a part of)
  vec2 uv;     // The texture coordinate of the vertex
};

// GPUMesh struct for holding geometry information which we'll send to the GPU
struct GPUMesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  GPUBuffer vertexBuffer;
  GPUBuffer indexBuffer;
};

struct ObjLoadData {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warningMsg;
  std::string errorMsg;
};

// Function to go along with the GPU mesh to create it
GPUMesh createGPUMesh(VmaAllocator allocator, std::vector<Vertex> vertices,
                      std::vector<uint32_t> indices);

// A little function that allows us to load mesh data from a .obj file!
GPUMesh loadMeshFromObj(VmaAllocator allocator, const char *path);

// Quick and simply triangle test mesh creation function for testing
GPUMesh createTriangleTestMesh(VmaAllocator allocator);
