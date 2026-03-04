#include "chunk.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

Chunk::Chunk(glm::ivec3 position)
    : chunkPosition(position), hasTriggeredMeshUpdate(true), currentLOD(LOD_0),
      status(ChunkState::UNINITIALIZED), VAO(0), glResourcesInitialized(false) {
  for (int i = 0; i < 4; ++i) {
    lodMeshNeedsUpdate[i] = true;
    VBO[i] = 0;
  }

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
    glDeleteBuffers(4, VBO);
  }
}

void Chunk::setVoxel(int x, int y, int z, uint8_t voxelID) {
  if (x >= 0 && x < CHUNK_WIDTH && y >= 0 && y < CHUNK_HEIGHT && z >= 0 &&
      z < CHUNK_DEPTH) {
    voxelIDs[x][y][z] = voxelID;
    hasTriggeredMeshUpdate = true;
    // Mark all LODs as needing update
    setLODUpdateAll();
  }
}

uint8_t Chunk::sampleVoxelBlock(int baseX, int baseY, int baseZ,
                                int blockSize) {
  for (int x = 0; x < blockSize && baseX + x < CHUNK_WIDTH; ++x) {
    for (int y = 0; y < blockSize && baseY + y < CHUNK_HEIGHT; ++y) {
      for (int z = 0; z < blockSize && baseZ + z < CHUNK_DEPTH; ++z) {
        uint8_t voxel = voxelIDs[baseX + x][baseY + y][baseZ + z];
        if (voxel != Voxel::EMPTY)
          return voxel;
      }
    }
  }
  return Voxel::EMPTY;
}

bool Chunk::shouldRenderFaceLOD(uint8_t downsampledVoxels[16][16][16], int x,
                                int y, int z, int d, int direction, int width,
                                int height, int depth) const {
  int nx = x + (d == 0 ? direction : 0);
  int ny = y + (d == 1 ? direction : 0);
  int nz = z + (d == 2 ? direction : 0);

  if (nx >= 0 && nx < width && ny >= 0 && ny < height && nz >= 0 &&
      nz < depth) {
    return downsampledVoxels[nx][ny][nz] == Voxel::EMPTY;
  }

  int step = 1 << (width == 16 ? 0 : width == 8 ? 1 : width == 4 ? 2 : 3);
  int neighborDir = getNeighborDirection(d, direction);
  int blockX, blockY, blockZ;

  if (nx < 0) {
    blockX = CHUNK_WIDTH - step;
    blockY = y * step;
    blockZ = z * step;
  } else if (nx >= width) {
    blockX = 0;
    blockY = y * step;
    blockZ = z * step;
  } else if (ny < 0) {
    blockX = x * step;
    blockY = CHUNK_HEIGHT - step;
    blockZ = z * step;
  } else if (ny >= height) {
    blockX = x * step;
    blockY = 0;
    blockZ = z * step;
  } else if (nz < 0) {
    blockX = x * step;
    blockY = y * step;
    blockZ = CHUNK_DEPTH - step;
  } else if (nz >= depth) {
    blockX = x * step;
    blockY = y * step;
    blockZ = 0;
  } else {
    printf("Logic error in shouldRenderFaceLOD\n");
    return true;
  }

  Chunk *neighborChunk = neighbors[neighborDir].load(std::memory_order_acquire);
  if (neighborChunk == nullptr)
    return true;

  uint8_t neighborBlock =
      neighborChunk->sampleVoxelBlock(blockX, blockY, blockZ, step);
  return neighborBlock == Voxel::EMPTY;
}

void Chunk::greedyMeshAxisLOD(std::vector<Voxel::PackedVoxel> &meshData,
                              uint8_t downsampledVoxels[16][16][16], int width,
                              int height, int depth, int voxelScale, int d,
                              int u, int v, int direction,
                              Voxel::VoxelFace faceDir) {
  int dims[3] = {width, height, depth};
  std::vector<uint8_t> mask(dims[u] * dims[v], 0);

  for (int depthLayer = 0; depthLayer < dims[d]; ++depthLayer) {
    std::fill(mask.begin(), mask.end(), 0);

    for (int uu = 0; uu < dims[u]; ++uu) {
      for (int vv = 0; vv < dims[v]; ++vv) {
        int x = (d == 0) ? depthLayer : (u == 0) ? uu : vv;
        int y = (d == 1) ? depthLayer : (u == 1) ? uu : vv;
        int z = (d == 2) ? depthLayer : (u == 2) ? uu : vv;

        uint8_t voxelID = downsampledVoxels[x][y][z];

        if (voxelID != Voxel::EMPTY &&
            shouldRenderFaceLOD(downsampledVoxels, x, y, z, d, direction, width,
                                height, depth)) {
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

        // All defined data per point here
        meshData.push_back(Voxel::packVertexData(
            x * voxelScale, y * voxelScale, z * voxelScale,
            meshWidth * voxelScale, meshHeight * voxelScale, voxelID, faceDir,
            currentLOD));

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

void Chunk::generateMeshLOD(LODLevel lod) {
  if (!lodMeshNeedsUpdate[lod])
    return;
  status = ChunkState::GENERATING;

  int step = 1 << lod;
  int adjustedWidth = CHUNK_WIDTH / step;
  int adjustedHeight = CHUNK_HEIGHT / step;
  int adjustedDepth = CHUNK_DEPTH / step;

  uint8_t downsampledVoxels[16][16][16];
  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      for (int z = 0; z < 16; ++z) {
        downsampledVoxels[x][y][z] = Voxel::EMPTY;
      }
    }
  }

  for (int x = 0; x < adjustedWidth; ++x) {
    for (int y = 0; y < adjustedHeight; ++y) {
      for (int z = 0; z < adjustedDepth; ++z) {
        uint8_t voxelID = sampleVoxelBlock(x * step, y * step, z * step, step);
        downsampledVoxels[x][y][z] = voxelID;
      }
    }
  }

  std::vector<Voxel::PackedVoxel> newMeshData;

  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 0, 1, 2, -1,
                    Voxel::LEFT);
  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 0, 1, 2, +1,
                    Voxel::RIGHT);
  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 1, 0, 2, -1,
                    Voxel::BOTTOM);
  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 1, 0, 2, +1,
                    Voxel::TOP);
  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 2, 0, 1, -1,
                    Voxel::BACK);
  greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth,
                    adjustedHeight, adjustedDepth, step, 2, 0, 1, +1,
                    Voxel::FRONT);

  meshData[lod] = std::move(newMeshData);
  lodMeshNeedsUpdate[lod] = false;
  status = ChunkState::WAITING_FOR_UPLOAD;
}

void Chunk::initGLResources() {
  if (glResourcesInitialized)
    return;

  glGenVertexArrays(1, &VAO);
  glGenBuffers(4, VBO);

  glBindVertexArray(VAO);
  glEnableVertexAttribArray(0);
  glVertexAttribDivisor(0, 1);

  glResourcesInitialized = true;
}

bool Chunk::updateVBO(LODLevel lod) {
  const std::vector<Voxel::PackedVoxel> &data = meshData[lod];

  glBindBuffer(GL_ARRAY_BUFFER, VBO[lod]);
  glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(uint32_t), (void *)0);
  if (data.empty()) {
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  } else {
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(Voxel::PackedVoxel),
                 data.data(), GL_DYNAMIC_DRAW);
  }
  // glBindBuffer(GL_ARRAY_BUFFER, 0);

  lodMeshNeedsUpdate[lod] = false;

  return true;
}

void Chunk::bindAndDraw(LODLevel lod) {
  if (status != ChunkState::WAITING_FOR_UPLOAD && status != ChunkState::IDLE)
    return;
  ChunkState prevStatus = status;
  status = ChunkState::UPLOADING;
  initGLResources();

  glBindVertexArray(VAO);

  // Update VBOs if needed
  if (prevStatus == ChunkState::WAITING_FOR_UPLOAD) {
    updateVBO(lod);
  }

  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                        static_cast<GLsizei>(meshData[lod].size()));

  // glBindVertexArray(0);
  // glBindBuffer(GL_ARRAY_BUFFER, 0);

  status = ChunkState::IDLE;
}