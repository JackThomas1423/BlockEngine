#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "voxel.hpp"

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

#define MAX_HEIGHT CHUNK_HEIGHT * 7

enum LODLevel : uint8_t {
    LOD_0 = 0,  // Full detail (1 voxel = 1 unit)
    LOD_1 = 1,  // 2x2x2 voxels merged
    LOD_2 = 2,  // 4x4x4 voxels merged
    LOD_3 = 3   // 8x8x8 voxels merged
};

struct ChunkVertex {
    uint32_t packedData;
    uint32_t chunkOffset;
};

static inline uint64_t splitmix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

struct ChunkPositionHash {
    size_t operator()(const glm::ivec3& v) const noexcept {
        uint64_t h = 0;
        h ^= splitmix64((uint64_t)v.x);
        h ^= splitmix64((uint64_t)v.y + 0x9e3779b97f4a7c15ull);
        h ^= splitmix64((uint64_t)v.z + 0xbf58476d1ce4e5b9ull);
        return (size_t)h;
    }
};

inline uint32_t packChunkOffset(glm::ivec3 chunkPosition, LODLevel lod) {
    uint32_t offset = 0;
    offset |= ((chunkPosition.x + 128) & 0xFF) << 0;
    offset |= ((chunkPosition.y + 128) & 0xFF) << 8;
    offset |= ((chunkPosition.z + 128) & 0xFF) << 16;
    offset |= ((lod + 128)             & 0xFF) << 24;
    return offset;
}

class Chunk;

typedef std::unordered_map<glm::ivec3, Chunk*, ChunkPositionHash> ChunkMap;

class Chunk {
    private:
        uint8_t voxelIDs[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        std::vector<Voxel::PackedVoxel> meshData[4];  // One per LOD level
        bool meshNeedsUpdate[4];
        bool lodGenerated[4];
        
        bool shouldRenderFace(int x, int y, int z, int d, int direction) const;
        void greedyMeshAxis(std::vector<Voxel::PackedVoxel>& meshData, int d, int u, int v, int direction, Voxel::VoxelFace faceDir);
        
        // LOD-specific methods
        uint8_t sampleVoxelBlock(int baseX, int baseY, int baseZ, int blockSize);
        void greedyMeshAxisLOD(std::vector<Voxel::PackedVoxel>& meshData, 
                               uint8_t downsampledVoxels[16][16][16],
                               int width, int height, int depth,
                               int voxelScale, int d, int u, int v, 
                               int direction, Voxel::VoxelFace faceDir);
        bool shouldRenderFaceLOD(uint8_t downsampledVoxels[16][16][16],
                                 int x, int y, int z, int d, int direction,
                                 int width, int height, int depth) const;
        
    public:
        glm::ivec3 chunkPosition;
        ChunkMap* map_ptr;
        bool hasTriggeredMeshUpdate;
        LODLevel currentLOD;

        Chunk(glm::ivec3 position, ChunkMap* map = nullptr);
        void setVoxel(int x, int y, int z, uint8_t voxelID);
        uint8_t getVoxel_ID(int x, int y, int z) const;
        uint8_t getAdjChunkVoxel_ID(Voxel::VoxelFace adjChunkDir, int x, int y, int z) const;
        
        // LOD methods
        void generateMeshLOD(LODLevel lod);
        const std::vector<Voxel::PackedVoxel>& getMeshData(LODLevel lod);
        void setLODLevel(LODLevel lod) { currentLOD = lod; }
        LODLevel getLODLevel() const { return currentLOD; }
        
        // Legacy method for LOD_0
        std::vector<Voxel::PackedVoxel> createMeshData();
};