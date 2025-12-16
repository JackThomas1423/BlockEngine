#include "chunk.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

Chunk::Chunk(glm::ivec3 position) : chunkPosition(position), hasTriggeredMeshUpdate(true)
{
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                voxelIDs[x][y][z] = 0;
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
    return 0;
}

bool Chunk::isFaceVisible(int x, int y, int z, int dx, int dy, int dz) const {
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;
    
    if (nx < 0 || nx >= CHUNK_WIDTH ||
        ny < 0 || ny >= CHUNK_HEIGHT ||
        nz < 0 || nz >= CHUNK_DEPTH) {
        return true;
    }
    
    return voxelIDs[nx][ny][nz] == Voxel::EMPTY;
}

bool Chunk::shouldRenderFace(int x, int y, int z, int d, int direction) const {
    int nx = x + (d == 0 ? direction : 0);
    int ny = y + (d == 1 ? direction : 0);
    int nz = z + (d == 2 ? direction : 0);

    // Log the coordinates being checked
    std::cout << "Checking face at: (" << x << ", " << y << ", " << z << ") with neighbor (" 
              << nx << ", " << ny << ", " << nz << ") in direction " << direction << std::endl;

    // Always render faces on the edge of chunk boundaries
    if (nx < 0 || nx >= CHUNK_WIDTH ||
        ny < 0 || ny >= CHUNK_HEIGHT ||
        nz < 0 || nz >= CHUNK_DEPTH) {
        std::cout << "Rendering face on chunk boundary." << std::endl;
        return true;
    }

    // Check if the neighboring voxel is empty
    bool isEmpty = voxelIDs[nx][ny][nz] == 0;
    std::cout << "Neighbor voxel ID: " << (int)voxelIDs[nx][ny][nz] << " (empty: " << isEmpty << ")" << std::endl;
    return isEmpty;
}

void Chunk::greedyMeshAxis(std::vector<Voxel::PackedVoxel>& meshData, int d, int u, int v, int direction, Voxel::VoxelFace faceDir) {
    int dims[3] = {CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH};
    uint8_t mask[CHUNK_WIDTH * CHUNK_HEIGHT];
    
    for (int depth = 0; depth < dims[d]; ++depth) {
        std::memset(mask, 0, sizeof(mask));
        
        // Build mask
        for (int uu = 0; uu < dims[u]; ++uu) {
            for (int vv = 0; vv < dims[v]; ++vv) {
                int x = (d == 0) ? depth : (u == 0) ? uu : vv;
                int y = (d == 1) ? depth : (u == 1) ? uu : vv;
                int z = (d == 2) ? depth : (u == 2) ? uu : vv;
                
                uint8_t voxelID = voxelIDs[x][y][z];
                
                if (voxelID != Voxel::EMPTY && shouldRenderFace(x, y, z, d, direction)) {
                    mask[uu + vv * dims[u]] = voxelID;
                }
            }
        }
        
        // Greedy mesh
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
