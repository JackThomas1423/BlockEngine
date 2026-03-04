#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>

#include <atomic>
#include <array>
#include <vector>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

#include "voxel.hpp"

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

#define MAX_HEIGHT CHUNK_HEIGHT * 7

enum LODLevel : uint8_t {
    LOD_0 = 0,           // Full detail (1 voxel = 1 unit)
    LOD_1 = 1,           // 2x2x2 voxels merged
    LOD_2 = 2,           // 4x4x4 voxels merged
    LOD_3 = 3            // 8x8x8 voxels merged
};

enum NeighborDirection : int {
    NEIGHBOR_POS_X = 0,  // Right
    NEIGHBOR_NEG_X = 1,  // Left
    NEIGHBOR_POS_Y = 2,  // Top
    NEIGHBOR_NEG_Y = 3,  // Bottom
    NEIGHBOR_POS_Z = 4,  // Front
    NEIGHBOR_NEG_Z = 5   // Back
};

enum ChunkState : uint8_t {
    UNINITIALIZED            = 0,   // Just created, no data
    GENERATING               = 1,   // Meshing, remeshing, or terrain gen in progress
    WAITING_FOR_UPLOAD       = 2,   // Mesh generated, waiting to be sent to GPU or binded to VAO
    UPLOADING                = 3,   // Being uploaded or drawn
    IDLE                     = 4,   // Fully loaded
    MARKED_FOR_DELETION      = 5,   // Waiting for all references to be released before deletion
    WAITING_FOR_MESH_UPDATE  = 6,   // Waiting for mesh update to get picked up
};

inline int getNeighborDirection(int d, int direction) {
    // d = axis (0=X, 1=Y, 2=Z)
    // direction = -1 or +1
    if (d == 0) return (direction > 0) ? NEIGHBOR_POS_X : NEIGHBOR_NEG_X;
    if (d == 1) return (direction > 0) ? NEIGHBOR_POS_Y : NEIGHBOR_NEG_Y;
    return (direction > 0) ? NEIGHBOR_POS_Z : NEIGHBOR_NEG_Z;
}

struct ChunkVertex {
    uint32_t packedData;
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

class Chunk;

typedef std::unordered_map<glm::ivec3, Chunk*, ChunkPositionHash> ChunkMap;

class Chunk {
    private:
        uint8_t voxelIDs[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        std::vector<Voxel::PackedVoxel> meshData[4];  // One per LOD level (vertex data only)
        bool lodMeshNeedsUpdate[4];  // Track if mesh needs regeneration for each LOD

        // Order: +X, -X, +Y, -Y, +Z, -Z
        std::atomic<Chunk*> neighbors[6] {nullptr};
        
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
        bool hasTriggeredMeshUpdate;
        LODLevel currentLOD;
        ChunkState status;

        unsigned int VAO;
        unsigned int VBO[4];          // Vertex data VBOs (one per LOD)
        bool glResourcesInitialized;

        Chunk(glm::ivec3 position);
        ~Chunk();
        void setVoxel(int x, int y, int z, uint8_t voxelID);
        uint8_t getVoxel_ID(int x, int y, int z) const;
        uint8_t getAdjChunkVoxel_ID(Voxel::VoxelFace adjChunkDir, int x, int y, int z) const;
        bool getLODMeshNeedsUpdate(LODLevel lod) { return lodMeshNeedsUpdate[lod]; }
        
        // LOD methods
        void generateMeshLOD(LODLevel lod);
        void initGLResources();  // Initialize OpenGL resources on main thread
        bool updateVBO(LODLevel lod);  // Returns true if upload happened
        void bindAndDraw(LODLevel lod);  // Bind VAO and draw for specific LOD

        void setLODLevel(LODLevel lod) {currentLOD = lod;}
        LODLevel getLODLevel() const { return currentLOD; }
        void setLODMeshNeedsUpdate(LODLevel lod) {lodMeshNeedsUpdate[lod] = true;}
        void setLODUpdateAll() {
            for(int i = 0; i < 4; ++i) {
                lodMeshNeedsUpdate[i] = true;
            }
        }
        
        size_t getVertexCount(LODLevel lod) const { return lodMeshNeedsUpdate[lod] ? 0 :meshData[lod].size(); }

        // Neighbor calls
        void setNeighbor(int direction, Chunk* neighbor) {
            neighbors[direction].store(neighbor, std::memory_order_release);
        }
    
        Chunk* getNeighbor(int direction) const {
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