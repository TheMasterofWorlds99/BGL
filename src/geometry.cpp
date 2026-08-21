#include "../include/geometry.hpp"
#include "../include/memory.hpp"
#include "../include/engine.hpp"
#include <cstring>
#include <iostream>
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <vector>

/*

   geometry.cpp contains the implementations for geometry.hpp — turning mesh
   data into GPU buffers, loading .obj files, and small test mesh helpers.

*/

GPUMesh createGPUMesh(VmaAllocator allocator, std::vector<Vertex> vertices,
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

GPUMesh loadMeshFromObj(VmaAllocator allocator, const char *path) {
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

  // Loop through all shapes in the file. Faces may be triangles, quads or
  // n-gons; tinyobj reports each face's vertex count in num_face_vertices.
  // Each face is fan-triangulated (0,1,2), (0,2,3), (0,3,4), ... so a quad
  // becomes two triangles instead of corrupting the flat index stream.
  for (const auto &shape : data.shapes) {
    const auto &faceIndices = shape.mesh.indices;
    const auto &faceSizes = shape.mesh.num_face_vertices;

    size_t i = 0; // Running offset into faceIndices
    for (size_t face = 0; face < faceSizes.size(); face++) {
      const uint32_t faceSize = faceSizes[face];

      for (uint32_t t = 1; t + 1 < faceSize; t++) {
        tinyobj::index_t tri[3] = {faceIndices[i + 0], faceIndices[i + t],
                                   faceIndices[i + t + 1]};

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
      i += faceSize;
    }
  }

  // Normalize the averaged (smooth) normals
  for (Vertex &vertex : vertices) {
    vertex.normal = glm::normalize(vertex.normal);
  }

  return createGPUMesh(allocator, vertices, indices);
}

GPUMesh createTriangleTestMesh(VmaAllocator allocator) {
  // Positons in NDC (normalized device coordinates)
  std::vector<Vertex> vertices = {Vertex{.pos = vec3(-1, 1, 0)},
                                  Vertex{.pos = vec3(1, 1, 0)},
                                  Vertex{.pos = vec3(0, -1, 0)}};

  // Order to connect vertices to form CCW triangle
  std::vector<uint32_t> indices = {0, 1, 2};
  return createGPUMesh(allocator, vertices, indices);
}

// ---------------------------------------------------------------------------
// glTF (.glb) loading via tinygltf
// ---------------------------------------------------------------------------

#include <tiny_gltf.h>

// Read one attribute (e.g. "POSITION") from a primitive into floats.
// Handles float and normalized u16/u8 component types, and bufferView strides.
static bool readGlbAttribute(const tinygltf::Model &model,
                             const tinygltf::Primitive &prim,
                             const std::string &name,
                             std::vector<float> &out, int &numComps) {
  auto it = prim.attributes.find(name);
  if (it == prim.attributes.end())
    return false;
  const tinygltf::Accessor &acc = model.accessors[it->second];
  const tinygltf::BufferView &bv = model.bufferViews[acc.bufferView];
  const tinygltf::Buffer &buf = model.buffers[bv.buffer];

  numComps = tinygltf::GetNumComponentsInType(acc.type);
  size_t compSize = tinygltf::GetComponentSizeInBytes(acc.componentType);
  size_t stride = bv.byteStride ? bv.byteStride : numComps * compSize;
  const unsigned char *base = buf.data.data() + bv.byteOffset + acc.byteOffset;

  out.resize(acc.count * numComps);
  for (size_t i = 0; i < acc.count; i++) {
    const unsigned char *p = base + i * stride;
    for (int c = 0; c < numComps; c++) {
      float v = 0.0f;
      switch (acc.componentType) {
      case TINYGLTF_COMPONENT_TYPE_FLOAT:
        std::memcpy(&v, p + c * compSize, 4);
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: { // normalized
        uint16_t raw;
        std::memcpy(&raw, p + c * 2, 2);
        v = raw / 65535.0f;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: { // normalized
        v = p[c] / 255.0f;
        break;
      }
      default:
        break;
      }
      out[i * numComps + c] = v;
    }
  }
  return true;
}

// Read the primitive's index buffer (u8/u16/u32) into uint32s
static bool readGlbIndices(const tinygltf::Model &model,
                           const tinygltf::Primitive &prim,
                           std::vector<uint32_t> &out) {
  if (prim.indices < 0)
    return false;
  const tinygltf::Accessor &acc = model.accessors[prim.indices];
  const tinygltf::BufferView &bv = model.bufferViews[acc.bufferView];
  const tinygltf::Buffer &buf = model.buffers[bv.buffer];
  const unsigned char *base = buf.data.data() + bv.byteOffset + acc.byteOffset;

  out.resize(acc.count);
  for (size_t i = 0; i < acc.count; i++) {
    switch (acc.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      std::memcpy(&out[i], base + i * 4, 4);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      uint16_t s;
      std::memcpy(&s, base + i * 2, 2);
      out[i] = s;
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      out[i] = base[i];
      break;
    default:
      break;
    }
  }
  return true;
}

GPUMesh loadMeshFromGlb(VmaAllocator allocator, const char *path) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err, warn;
  if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
    std::cerr << "GLB load failed: " << err << "\n";
    exit(EXIT_FAILURE);
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (const auto &mesh : model.meshes) {
    for (const auto &prim : mesh.primitives) {
      std::vector<float> pos, nrm, uv;
      int npos = 0, nnrm = 0, nuv = 0;
      if (!readGlbAttribute(model, prim, "POSITION", pos, npos))
        continue;
      bool hasNrm = readGlbAttribute(model, prim, "NORMAL", nrm, nnrm);
      bool hasUv = readGlbAttribute(model, prim, "TEXCOORD_0", uv, nuv);

      std::vector<uint32_t> primIndices;
      readGlbIndices(model, prim, primIndices);

      uint32_t base = static_cast<uint32_t>(vertices.size());
      size_t count = pos.size() / npos;
      for (size_t i = 0; i < count; i++) {
        Vertex v{};
        v.pos = {pos[i * npos + 0], pos[i * npos + 1], pos[i * npos + 2]};
        if (hasNrm)
          v.normal = {nrm[i * nnrm + 0], nrm[i * nnrm + 1], nrm[i * nnrm + 2]};
        if (hasUv)
          v.uv = {uv[i * nuv + 0], uv[i * nuv + 1]};
        vertices.push_back(v);
      }
      for (uint32_t idx : primIndices)
        indices.push_back(base + idx);
    }
  }

  return createGPUMesh(allocator, vertices, indices);
}

// Build a GPU mesh from one glTF primitive (shared by the glb loaders)
static GPUMesh meshFromPrimitive(VmaAllocator allocator,
                                 const tinygltf::Model &model,
                                 const tinygltf::Primitive &prim) {
  std::vector<float> pos, nrm, uv;
  int npos = 0, nnrm = 0, nuv = 0;
  if (!readGlbAttribute(model, prim, "POSITION", pos, npos))
    return createGPUMesh(allocator, {}, {});
  bool hasNrm = readGlbAttribute(model, prim, "NORMAL", nrm, nnrm);
  bool hasUv = readGlbAttribute(model, prim, "TEXCOORD_0", uv, nuv);

  std::vector<uint32_t> primIndices;
  readGlbIndices(model, prim, primIndices);

  std::vector<Vertex> vertices;
  vertices.reserve(pos.size() / npos);
  size_t count = pos.size() / npos;
  for (size_t i = 0; i < count; i++) {
    Vertex v{};
    // Negate Y: this glTF's Y axis is inverted relative to the engine's
    // camera convention (the reflection also fixes the winding, so no index
    // swap is needed).
    v.pos = {pos[i * npos + 0], -pos[i * npos + 1], pos[i * npos + 2]};
    if (hasNrm)
      v.normal = {nrm[i * nnrm + 0], nrm[i * nnrm + 1], nrm[i * nnrm + 2]};
    if (hasUv)
      v.uv = {uv[i * nuv + 0], uv[i * nuv + 1]};
    vertices.push_back(v);
  }
  return createGPUMesh(allocator, vertices, primIndices);
}

std::vector<GlbPrimitive> loadGlb(Engine &engine, const char *path) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err, warn;
  if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
    std::cerr << "GLB load failed: " << err << "\n";
    exit(EXIT_FAILURE);
  }

  std::vector<GlbPrimitive> out;
  for (const auto &mesh : model.meshes) {
    for (const auto &prim : mesh.primitives) {
      GlbPrimitive g;
      g.mesh = meshFromPrimitive(engine.allocator, model, prim);

      // Base-color texture from the primitive's material
      if (prim.material >= 0) {
        const auto &mat = model.materials[prim.material];
        int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIndex >= 0 && texIndex < static_cast<int>(model.textures.size())) {
          int imgIndex = model.textures[texIndex].source;
          if (imgIndex >= 0 && imgIndex < static_cast<int>(model.images.size())) {
            const auto &img = model.images[imgIndex];
            // tinygltf decodes images to RGBA8 via stb
            g.baseColor = createTexture(engine, img.width, img.height,
                                        img.image.data());
            g.hasTexture = true;
          }
        }
      }
      out.push_back(std::move(g));
    }
  }
  return out;
}
