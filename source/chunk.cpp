#include "chunk.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

Chunk::Chunk(glm::ivec3 position)
    : meshNeedsUpdate(true), chunkPosition(position),
      status(ChunkState::UNINITIALIZED),
      VAO(0), VBO(0), glResourcesInitialized(false) {

  for (int x = 0; x < CHUNK_WIDTH; ++x) {
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
      for (int z = 0; z < CHUNK_DEPTH; ++z) {
        voxelIDs[x][y][z] = Voxel::EMPTY;
      }
    }
  }
}

Chunk::~Chunk() {
  if (glResourcesInitialized) {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
  }
}

void Chunk::setVoxel(int x, int y, int z, uint8_t voxelID) {
  if (x >= 0 && x < CHUNK_WIDTH && y >= 0 && y < CHUNK_HEIGHT && z >= 0 &&
      z < CHUNK_DEPTH) {
    voxelIDs[x][y][z] = voxelID;
    meshNeedsUpdate = true;
  }
}

bool Chunk::shouldRenderFace(int x, int y, int z, int d, int direction) const {
  int nx = x + (d == 0 ? direction : 0);
  int ny = y + (d == 1 ? direction : 0);
  int nz = z + (d == 2 ? direction : 0);

  if (nx >= 0 && nx < CHUNK_WIDTH && ny >= 0 && ny < CHUNK_HEIGHT &&
      nz >= 0 && nz < CHUNK_DEPTH) {
    return voxelIDs[nx][ny][nz] == Voxel::EMPTY;
  }

  int neighborDir = getNeighborDirection(d, direction);
  Chunk *neighborChunk = neighbors[neighborDir].load(std::memory_order_acquire);
  if (neighborChunk == nullptr)
    return true;

  int bx = (nx < 0) ? CHUNK_WIDTH - 1  : (nx >= CHUNK_WIDTH)  ? 0 : nx;
  int by = (ny < 0) ? CHUNK_HEIGHT - 1 : (ny >= CHUNK_HEIGHT) ? 0 : ny;
  int bz = (nz < 0) ? CHUNK_DEPTH - 1  : (nz >= CHUNK_DEPTH)  ? 0 : nz;

  return neighborChunk->voxelIDs[bx][by][bz] == Voxel::EMPTY;
}

void Chunk::greedyMeshAxis(std::vector<Voxel::PackedVoxel> &meshData, int d,
                           int u, int v, int direction,
                           Voxel::VoxelFace faceDir) {
  int dims[3] = {CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH};
  std::vector<uint8_t> mask(dims[u] * dims[v], 0);

  for (int depthLayer = 0; depthLayer < dims[d]; ++depthLayer) {
    std::fill(mask.begin(), mask.end(), 0);

    for (int uu = 0; uu < dims[u]; ++uu) {
      for (int vv = 0; vv < dims[v]; ++vv) {
        int x = (d == 0) ? depthLayer : (u == 0) ? uu : vv;
        int y = (d == 1) ? depthLayer : (u == 1) ? uu : vv;
        int z = (d == 2) ? depthLayer : (u == 2) ? uu : vv;

        uint8_t voxelID = voxelIDs[x][y][z];

        if (voxelID != Voxel::EMPTY &&
            shouldRenderFace(x, y, z, d, direction)) {
          mask[uu + vv * dims[u]] = voxelID;
        } else {
          mask[uu + vv * dims[u]] = 0;
        }
      }
    }

    for (int uu = 0; uu < dims[u]; ++uu) {
      for (int vv = 0; vv < dims[v];) {
        uint8_t voxelID = mask[uu + vv * dims[u]];

        if (voxelID == 0) {
          ++vv;
          continue;
        }

        int meshWidth = 1;
        while (vv + meshWidth < dims[v] &&
               mask[uu + (vv + meshWidth) * dims[u]] == voxelID) {
          ++meshWidth;
        }

        int meshHeight = 1;
        bool canExpand = true;
        while (uu + meshHeight < dims[u] && canExpand) {
          for (int k = 0; k < meshWidth; ++k) {
            if (mask[(uu + meshHeight) + (vv + k) * dims[u]] != voxelID) {
              canExpand = false;
              break;
            }
          }
          if (canExpand)
            ++meshHeight;
        }

        int x = (d == 0) ? depthLayer : (u == 0) ? uu : vv;
        int y = (d == 1) ? depthLayer : (u == 1) ? uu : vv;
        int z = (d == 2) ? depthLayer : (u == 2) ? uu : vv;

        meshData.push_back(Voxel::packVertexData(
            x, y, z, meshWidth, meshHeight, voxelID, faceDir));

        for (int hh = 0; hh < meshHeight; ++hh) {
          for (int ww = 0; ww < meshWidth; ++ww) {
            mask[(uu + hh) + (vv + ww) * dims[u]] = 0;
          }
        }

        vv += meshWidth;
      }
    }
  }
}

void Chunk::generateMesh() {
  if (markedForDeletion)
    return;
  if (!meshNeedsUpdate)
    return;
  status = ChunkState::GENERATING;

  std::vector<Voxel::PackedVoxel> newMeshData;

  greedyMeshAxis(newMeshData, 0, 1, 2, -1, Voxel::LEFT);
  greedyMeshAxis(newMeshData, 0, 1, 2, +1, Voxel::RIGHT);
  greedyMeshAxis(newMeshData, 1, 0, 2, -1, Voxel::BOTTOM);
  greedyMeshAxis(newMeshData, 1, 0, 2, +1, Voxel::TOP);
  greedyMeshAxis(newMeshData, 2, 0, 1, -1, Voxel::BACK);
  greedyMeshAxis(newMeshData, 2, 0, 1, +1, Voxel::FRONT);

  meshData = std::move(newMeshData);
  meshNeedsUpdate = false;
  status = ChunkState::WAITING_FOR_UPLOAD;
}

void Chunk::initGLResources() {
  if (glResourcesInitialized)
    return;

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glEnableVertexAttribArray(0);
  glVertexAttribDivisor(0, 1);

  glResourcesInitialized = true;
}

bool Chunk::updateVBO() {
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(uint32_t), (void *)0);
  if (meshData.empty()) {
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  } else {
    glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(Voxel::PackedVoxel),
                 meshData.data(), GL_DYNAMIC_DRAW);
  }

  return true;
}

void Chunk::bindAndDraw() {
  if (markedForDeletion)
    return;
  if (status != ChunkState::WAITING_FOR_UPLOAD && status != ChunkState::IDLE)
    return;
  ChunkState prevStatus = status;
  status = ChunkState::UPLOADING;
  initGLResources();

  glBindVertexArray(VAO);

  if (prevStatus == ChunkState::WAITING_FOR_UPLOAD) {
    updateVBO();
  }

  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                        static_cast<GLsizei>(meshData.size()));

  status = ChunkState::IDLE;
}