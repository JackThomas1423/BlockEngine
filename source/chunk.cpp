#include "chunk.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

Chunk::Chunk(glm::ivec3 position, ChunkMap* map) : chunkPosition(position), map_ptr(map), hasTriggeredMeshUpdate(true)
{
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                voxelIDs[x][y][z] = Voxel::EMPTY;
            }
        }
    }
}

void Chunk::setVoxel(int x, int y, int z, uint8_t voxelID) {
    if (x >= 0 && x < CHUNK_WIDTH &&
        y >= 0 && y < CHUNK_HEIGHT &&
        z >= 0 && z < CHUNK_DEPTH) {
        voxelIDs[x][y][z] = voxelID;
        hasTriggeredMeshUpdate = true;
    }
}

uint8_t Chunk::getVoxel_ID(int x, int y, int z) const {
    if (x >= 0 && x < CHUNK_WIDTH &&
        y >= 0 && y < CHUNK_HEIGHT &&
        z >= 0 && z < CHUNK_DEPTH) {
        return voxelIDs[x][y][z];
    }
    return Voxel::MAGENTA; // Return a default value for out-of-bounds (Debug color)
}

uint8_t Chunk::getAdjChunkVoxel_ID(Voxel::VoxelFace adjChunkDir, int x, int y, int z) const {
    glm::ivec3 dir[6] = {
        glm::ivec3(0, 0, 1),   // FRONT  = 0 -> +Z
        glm::ivec3(0, 0, -1),  // BACK   = 1 -> -Z
        glm::ivec3(0, 1, 0),   // TOP    = 2 -> +Y
        glm::ivec3(0, -1, 0),  // BOTTOM = 3 -> -Y
        glm::ivec3(1, 0, 0),   // RIGHT  = 4 -> +X
        glm::ivec3(-1, 0, 0)   // LEFT   = 5 -> -X
    };
    glm::ivec3 neighborPos = chunkPosition + dir[adjChunkDir];
    auto it = map_ptr->find(neighborPos);
    if (it != map_ptr->end()) {
        Chunk* neighborChunk = it->second;
        return neighborChunk->getVoxel_ID(x, y, z);
    }
    // If no neighbor chunk exists, treat as empty (air)
    return Voxel::EMPTY;
}

bool Chunk::shouldRenderFace(int x, int y, int z, int d, int direction) const {
    int nx = x + (d == 0 ? direction : 0);
    int ny = y + (d == 1 ? direction : 0);
    int nz = z + (d == 2 ? direction : 0);
    
    // Within bounds - check this chunk
    if (nx >= 0 && nx < CHUNK_WIDTH &&
        ny >= 0 && ny < CHUNK_HEIGHT &&
        nz >= 0 && nz < CHUNK_DEPTH) {
        return voxelIDs[nx][ny][nz] == Voxel::EMPTY;
    }
    
    // Out of bounds - check adjacent chunk
    // When we go negative, we check the far side of the previous chunk
    // When we go positive, we check the near side of the next chunk
    Voxel::VoxelFace faceDir;
    int adjX = x;
    int adjY = y;
    int adjZ = z;
    
    if (d == 0) {
        // X-axis
        if (nx < 0) {
            faceDir = Voxel::LEFT;
            adjX = CHUNK_WIDTH - 1;
        } else {
            faceDir = Voxel::RIGHT;
            adjX = 0;
        }
    } else if (d == 1) {
        // Y-axis  
        if (ny < 0) {
            faceDir = Voxel::BOTTOM;
            adjY = CHUNK_HEIGHT - 1;
        } else {
            faceDir = Voxel::TOP;
            adjY = 0;
        }
    } else {
        // Z-axis
        if (nz < 0) {
            faceDir = Voxel::BACK;
            adjZ = CHUNK_DEPTH - 1;
        } else {
            faceDir = Voxel::FRONT;
            adjZ = 0;
        }
    }
    
    return getAdjChunkVoxel_ID(faceDir, adjX, adjY, adjZ) == Voxel::EMPTY;
}

void Chunk::greedyMeshAxis(std::vector<Voxel::PackedVoxel>& meshData, int d, int u, int v, int direction, Voxel::VoxelFace faceDir) {
    int dims[3] = {CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH};
    std::vector<uint8_t> mask(dims[u] * dims[v], 0);
    
    for (int depth = 0; depth < dims[d]; ++depth) {
        std::fill(mask.begin(), mask.end(), 0);
        
        // Build mask
        for (int uu = 0; uu < dims[u]; ++uu) {
            for (int vv = 0; vv < dims[v]; ++vv) {
                int x = (d == 0) ? depth : (u == 0) ? uu : vv;
                int y = (d == 1) ? depth : (u == 1) ? uu : vv;
                int z = (d == 2) ? depth : (u == 2) ? uu : vv;
                
                uint8_t voxelID = voxelIDs[x][y][z];
                
                if (voxelID != Voxel::EMPTY && shouldRenderFace(x, y, z, d, direction)) {
                    mask[uu + vv * dims[u]] = voxelID;
                } else {mask[uu + vv * dims[u]] = 0;}
            }
        }

        for (int uu = 0; uu < dims[u]; ++uu) {
            for (int vv = 0; vv < dims[v];) {
                uint8_t voxelID = mask[uu + vv * dims[u]];
                    
                if (voxelID == 0) {
                    ++vv;
                    continue;
                }
                    
                // Expand width
                int width = 1;
                while (vv + width < dims[v] && mask[uu + (vv + width) * dims[u]] == voxelID) {
                    ++width;
                }
                    
                // Expand height
                int height = 1;
                bool canExpand = true;
                while (uu + height < dims[u] && canExpand) {
                    for (int k = 0; k < width; ++k) {
                        if (mask[(uu + height) + (vv + k) * dims[u]] != voxelID) {
                            canExpand = false;
                            break;
                        }
                    }
                    if (canExpand) ++height;
                }
                    
                // Convert back to world coords
                int x = (d == 0) ? depth : (u == 0) ? uu : vv;
                int y = (d == 1) ? depth : (u == 1) ? uu : vv;
                int z = (d == 2) ? depth : (u == 2) ? uu : vv;
                
                meshData.push_back(Voxel::packVertexData(
                    x, y, z,
                    width, height,
                    voxelID, faceDir
                ));
                    
                // Clear mask
                for (int hh = 0; hh < height; ++hh) {
                    for (int ww = 0; ww < width; ++ww) {
                        mask[(uu + hh) + (vv + ww) * dims[u]] = 0;
                    }
                }
                
                vv += width;
            }
        }
    }
}

std::vector<Voxel::PackedVoxel> Chunk::createMeshData() {
    if (!hasTriggeredMeshUpdate) {
        return meshData;
    }

    std::vector<Voxel::PackedVoxel> newMeshData;
    
    greedyMeshAxis(newMeshData, 0, 1, 2, -1, Voxel::LEFT);
    greedyMeshAxis(newMeshData, 0, 1, 2, +1, Voxel::RIGHT);
    greedyMeshAxis(newMeshData, 1, 0, 2, -1, Voxel::BOTTOM);
    greedyMeshAxis(newMeshData, 1, 0, 2, +1, Voxel::TOP);
    greedyMeshAxis(newMeshData, 2, 0, 1, -1, Voxel::BACK);
    greedyMeshAxis(newMeshData, 2, 0, 1, +1, Voxel::FRONT);

    meshData = newMeshData;
    hasTriggeredMeshUpdate = false;
    return newMeshData;
}
