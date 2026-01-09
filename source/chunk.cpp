#include "chunk.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

Chunk::Chunk(glm::ivec3 position, ChunkMap* map) 
    : chunkPosition(position), map_ptr(map), hasTriggeredMeshUpdate(true), currentLOD(LOD_0)
{
    for (int i = 0; i < 4; ++i) {
        meshNeedsUpdate[i] = true;
        lodGenerated[i] = false;
    }
    
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
        // Mark all LODs as needing update
        for (int i = 0; i < 4; ++i) {
            meshNeedsUpdate[i] = true;
        }
    }
}

uint8_t Chunk::sampleVoxelBlock(int baseX, int baseY, int baseZ, int blockSize) {
    // Return first solid voxel found in the block
    for (int x = 0; x < blockSize && baseX + x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < blockSize && baseY + y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < blockSize && baseZ + z < CHUNK_DEPTH; ++z) {
                uint8_t voxel = voxelIDs[baseX + x][baseY + y][baseZ + z];
                if (voxel != Voxel::EMPTY) return voxel;
            }
        }
    }
    return Voxel::EMPTY;
}

bool Chunk::shouldRenderFaceLOD(uint8_t downsampledVoxels[16][16][16],
                                int x, int y, int z, int d, int direction,
                                int width, int height, int depth) const 
{
    int nx = x + (d == 0 ? direction : 0);
    int ny = y + (d == 1 ? direction : 0);
    int nz = z + (d == 2 ? direction : 0);
    
    // Within bounds - check downsampled array
    if (nx >= 0 && nx < width &&
        ny >= 0 && ny < height &&
        nz >= 0 && nz < depth) {
        return downsampledVoxels[nx][ny][nz] == Voxel::EMPTY;
    }
    
    // Out of bounds - assume empty (we'll improve this later with neighbor checks)
    return true;
}

void Chunk::greedyMeshAxisLOD(std::vector<Voxel::PackedVoxel>& meshData, 
                              uint8_t downsampledVoxels[16][16][16],
                              int width, int height, int depth,
                              int voxelScale, int d, int u, int v, 
                              int direction, Voxel::VoxelFace faceDir) 
{
    int dims[3] = {width, height, depth};
    std::vector<uint8_t> mask(dims[u] * dims[v], 0);
    
    for (int depthLayer = 0; depthLayer < dims[d]; ++depthLayer) {
        std::fill(mask.begin(), mask.end(), 0);
        
        // Build mask
        for (int uu = 0; uu < dims[u]; ++uu) {
            for (int vv = 0; vv < dims[v]; ++vv) {
                int x = (d == 0) ? depthLayer : (u == 0) ? uu : vv;
                int y = (d == 1) ? depthLayer : (u == 1) ? uu : vv;
                int z = (d == 2) ? depthLayer : (u == 2) ? uu : vv;
                
                uint8_t voxelID = downsampledVoxels[x][y][z];
                
                if (voxelID != Voxel::EMPTY && 
                    shouldRenderFaceLOD(downsampledVoxels, x, y, z, d, direction, width, height, depth)) {
                    mask[uu + vv * dims[u]] = voxelID;
                } else {
                    mask[uu + vv * dims[u]] = 0;
                }
            }
        }

        // Greedy meshing on mask
        for (int uu = 0; uu < dims[u]; ++uu) {
            for (int vv = 0; vv < dims[v];) {
                uint8_t voxelID = mask[uu + vv * dims[u]];
                    
                if (voxelID == 0) {
                    ++vv;
                    continue;
                }
                    
                // Expand width
                int meshWidth = 1;
                while (vv + meshWidth < dims[v] && mask[uu + (vv + meshWidth) * dims[u]] == voxelID) {
                    ++meshWidth;
                }
                    
                // Expand height
                int meshHeight = 1;
                bool canExpand = true;
                while (uu + meshHeight < dims[u] && canExpand) {
                    for (int k = 0; k < meshWidth; ++k) {
                        if (mask[(uu + meshHeight) + (vv + k) * dims[u]] != voxelID) {
                            canExpand = false;
                            break;
                        }
                    }
                    if (canExpand) ++meshHeight;
                }
                    
                // Convert back to world coords (scaled)
                int x = (d == 0) ? depthLayer : (u == 0) ? uu : vv;
                int y = (d == 1) ? depthLayer : (u == 1) ? uu : vv;
                int z = (d == 2) ? depthLayer : (u == 2) ? uu : vv;
                
                meshData.push_back(Voxel::packVertexData(
                    x * voxelScale,
                    y * voxelScale,
                    z * voxelScale,
                    meshWidth * voxelScale,
                    meshHeight * voxelScale,
                    voxelID, faceDir
                ));
                    
                // Clear mask
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
    if (!meshNeedsUpdate[lod]) return;
    
    int step = 1 << lod;  // LOD_0=1, LOD_1=2, LOD_2=4, LOD_3=8
    int adjustedWidth = CHUNK_WIDTH / step;
    int adjustedHeight = CHUNK_HEIGHT / step;
    int adjustedDepth = CHUNK_DEPTH / step;
    
    // Downsample voxel data
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
    
    // Run greedy meshing on downsampled data
    std::vector<Voxel::PackedVoxel> newMeshData;
    
    // The parameters are: meshData, downsampledVoxels, width, height, depth, voxelScale, d, u, v, direction, faceDir
    // d=depth axis, u=height axis, v=width axis (for greedy meshing)
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 0, 1, 2, -1, Voxel::LEFT);   // X-axis, -X direction
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 0, 1, 2, +1, Voxel::RIGHT);  // X-axis, +X direction
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 1, 0, 2, -1, Voxel::BOTTOM); // Y-axis, -Y direction
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 1, 0, 2, +1, Voxel::TOP);    // Y-axis, +Y direction
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 2, 0, 1, -1, Voxel::BACK);   // Z-axis, -Z direction
    greedyMeshAxisLOD(newMeshData, downsampledVoxels, adjustedWidth, adjustedHeight, adjustedDepth, step, 2, 0, 1, +1, Voxel::FRONT);  // Z-axis, +Z direction

    meshData[lod] = std::move(newMeshData);
    meshNeedsUpdate[lod] = false;
    lodGenerated[lod] = true;
}

const std::vector<Voxel::PackedVoxel>& Chunk::getMeshData(LODLevel lod) {
    if (!lodGenerated[lod] || meshNeedsUpdate[lod]) {
        generateMeshLOD(lod);
    }
    return meshData[lod];
}

std::vector<Voxel::PackedVoxel> Chunk::createMeshData() {
    // Legacy method - just use LOD_0
    return getMeshData(LOD_0);
}