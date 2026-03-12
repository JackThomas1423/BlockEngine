#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>


#include "voxel.hpp"

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

#define MAX_HEIGHT CHUNK_HEIGHT * 7

enum NeighborDirection : int {
  NEIGHBOR_POS_X = 0, // Right
  NEIGHBOR_NEG_X = 1, // Left
  NEIGHBOR_POS_Y = 2, // Top
  NEIGHBOR_NEG_Y = 3, // Bottom
  NEIGHBOR_POS_Z = 4, // Front
  NEIGHBOR_NEG_Z = 5  // Back
};

enum ChunkState : uint8_t {
  UNINITIALIZED = 0,
  GENERATING = 1,
  WAITING_FOR_UPLOAD = 2,
  UPLOADING = 3,
  IDLE = 4,
  WAITING_FOR_MESH_UPDATE = 5,
};

inline int getNeighborDirection(int d, int direction) {
  if (d == 0)
    return (direction > 0) ? NEIGHBOR_POS_X : NEIGHBOR_NEG_X;
  if (d == 1)
    return (direction > 0) ? NEIGHBOR_POS_Y : NEIGHBOR_NEG_Y;
  return (direction > 0) ? NEIGHBOR_POS_Z : NEIGHBOR_NEG_Z;
}

struct ChunkVertex {
  uint32_t packedData;
};

static inline uint64_t splitmix64(uint64_t x) {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

struct ChunkPositionHash {
  size_t operator()(const glm::ivec3 &v) const noexcept {
    uint64_t h = 0;
    h ^= splitmix64((uint64_t)v.x);
    h ^= splitmix64((uint64_t)v.y + 0x9e3779b97f4a7c15ull);
    h ^= splitmix64((uint64_t)v.z + 0xbf58476d1ce4e5b9ull);
    return (size_t)h;
  }
};

class Chunk;

typedef std::unordered_map<glm::ivec3, Chunk *, ChunkPositionHash> ChunkMap;

class Chunk {
private:
  uint8_t voxelIDs[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
  std::vector<Voxel::PackedVoxel> meshData;
  bool meshNeedsUpdate;

  // Order: +X, -X, +Y, -Y, +Z, -Z
  std::atomic<Chunk *> neighbors[6]{nullptr};

  bool shouldRenderFace(int x, int y, int z, int d, int direction) const;
  void greedyMeshAxis(std::vector<Voxel::PackedVoxel> &meshData, int d, int u,
                      int v, int direction, Voxel::VoxelFace faceDir);

public:
  glm::ivec3 chunkPosition;
  ChunkState status;
  std::atomic<bool> markedForDeletion{false};

  unsigned int VAO;
  unsigned int VBO;
  bool glResourcesInitialized;

  Chunk(glm::ivec3 position);
  ~Chunk();
  void setVoxel(int x, int y, int z, uint8_t voxelID);
  uint8_t getVoxel_ID(int x, int y, int z) const;
  uint8_t getAdjChunkVoxel_ID(Voxel::VoxelFace adjChunkDir, int x, int y,
                              int z) const;
  bool getMeshNeedsUpdate() const { return meshNeedsUpdate; }
  void setMeshNeedsUpdate() { meshNeedsUpdate = true; }

  void generateMesh();
  void initGLResources();
  bool updateVBO();
  void bindAndDraw();

  size_t getVertexCount() const {
    return meshNeedsUpdate ? 0 : meshData.size();
  }

  // Neighbor calls
  void setNeighbor(int direction, Chunk *neighbor) {
    neighbors[direction].store(neighbor, std::memory_order_release);
  }

  Chunk *getNeighbor(int direction) const {
    return neighbors[direction].load(std::memory_order_acquire);
  }

  void clearNeighbor(int direction) {
    neighbors[direction].store(nullptr, std::memory_order_release);
  }

  void clearAllNeighbors() {
    for (int i = 0; i < 6; ++i) {
      neighbors[i].store(nullptr, std::memory_order_release);
    }
  }
};