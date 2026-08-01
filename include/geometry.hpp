#pragma once

// Includes
#include "engine.hpp"

// Other Includes
#include "memory.hpp"
#include <cstdint>
#include <tiny_obj_loader.h>
#include <unordered_map>
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
inline GPUMesh createGPUMesh(VmaAllocator allocator,
                             std::vector<Vertex> vertices,
                             std::vector<uint32_t> indices) {
  // Calculate the sizes in bytes
  uint32_t vertexBufferSize =
      static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
  uint32_t indexBufferSize =
      static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

  // Create the vertex and index buffer's, and then make the mesh using them
  auto vertexBuffer =
      createGPUBufferWithData(allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              vertices.data(), vertexBufferSize);

  auto indexBuffer =
      createGPUBufferWithData(allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              indices.data(), indexBufferSize);

  GPUMesh mesh{
      .vertices = std::move(vertices),
      .indices = std::move(indices),
      .vertexBuffer = vertexBuffer,
      .indexBuffer = indexBuffer,
  };

  return mesh;
}

// A little function that allows us to load mesh data from a .obj file!
inline GPUMesh loadMeshFromObj(VmaAllocator allocator, const char *path) {
  // Load from file
  ObjLoadData data;
  bool success = tinyobj::LoadObj(&data.attrib, &data.shapes, &data.materials,
                                  &data.warningMsg, &data.errorMsg, path);

  if (!success) {
    std::cerr << "OBJ Load failed:\n" << data.errorMsg << '\n';
    exit(EXIT_FAILURE);
  }

  // Dedup by POSITION only. Meshes exported with per-face (flat) normals
  // would otherwise give ~3 vertices per face (one per corner) and silently
  // undo any decimation done in Blender. Normals are accumulated per position
  // (from the file, or per-face when the file has none) and averaged at the
  // end, which also gives smooth shading for flat exports.
  std::unordered_map<int32_t, uint32_t> vertexLookup; // position idx -> vertex
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  const bool hasFileNormals = !data.attrib.normals.empty();

  // Loop through all shapes in the file
  for (const auto &shape : data.shapes) {
    const auto &faceIndices = shape.mesh.indices;

    for (size_t i = 0; i < faceIndices.size(); i += 3) {
      tinyobj::index_t tri[3] = {faceIndices[i + 0], faceIndices[i + 1],
                                 faceIndices[i + 2]};

      // Compute face normal for the no-file-normal fallback
      glm::vec3 p0{data.attrib.vertices[tri[0].vertex_index * 3 + 0],
                   data.attrib.vertices[tri[0].vertex_index * 3 + 1],
                   data.attrib.vertices[tri[0].vertex_index * 3 + 2]};
      glm::vec3 p1{data.attrib.vertices[tri[1].vertex_index * 3 + 0],
                   data.attrib.vertices[tri[1].vertex_index * 3 + 1],
                   data.attrib.vertices[tri[1].vertex_index * 3 + 2]};
      glm::vec3 p2{data.attrib.vertices[tri[2].vertex_index * 3 + 0],
                   data.attrib.vertices[tri[2].vertex_index * 3 + 1],
                   data.attrib.vertices[tri[2].vertex_index * 3 + 2]};
      glm::vec3 faceNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

      for (int c = 0; c < 3; c++) {
        uint32_t vertexIndex;
        auto found = vertexLookup.find(tri[c].vertex_index);

        if (found != vertexLookup.end()) {
          vertexIndex = found->second;
        } else {
          vertexIndex = static_cast<uint32_t>(vertices.size());

          Vertex v{};
          v.pos = {data.attrib.vertices[tri[c].vertex_index * 3 + 0],
                   data.attrib.vertices[tri[c].vertex_index * 3 + 1],
                   data.attrib.vertices[tri[c].vertex_index * 3 + 2]};
          v.normal = glm::vec3(0.0f); // Accumulate below safely
          v.uv = glm::vec2(0.0f);

          vertices.push_back(v);
          vertexLookup.emplace(tri[c].vertex_index, vertexIndex);
        }

        // Accumulate this corner's normal (file normal, or the computed face
        // normal if the file has none) to average at the end.
        if (hasFileNormals && tri[c].normal_index >= 0) {
          vertices[vertexIndex].normal +=
              glm::vec3(data.attrib.normals[tri[c].normal_index * 3 + 0],
                        data.attrib.normals[tri[c].normal_index * 3 + 1],
                        data.attrib.normals[tri[c].normal_index * 3 + 2]);
        } else {
          vertices[vertexIndex].normal += faceNormal;
        }

        indices.push_back(vertexIndex);
      }
    }
  }

  // Normalize the averaged (smooth) normals
  for (Vertex &vertex : vertices) {
    vertex.normal = glm::normalize(vertex.normal);
  }

  return createGPUMesh(allocator, vertices, indices);
}

// Quick and simply triangle test mesh creation function for testing
inline GPUMesh createTriangleTestMesh(VmaAllocator allocator) {
  // Positons in NDC (normalized device coordinates)
  std::vector<Vertex> vertices = {Vertex{.pos = vec3(-1, 1, 0)},
                                  Vertex{.pos = vec3(1, 1, 0)},
                                  Vertex{.pos = vec3(0, -1, 0)}};

  // Order to connect vertices to form CCW triangle
  std::vector<uint32_t> indices = {0, 1, 2};
  return createGPUMesh(allocator, vertices, indices);
}
