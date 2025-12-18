#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "../Jack/voxel.hpp"

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

#define MAX_HEIGHT CHUNK_HEIGHT * 5

struct ChunkVertex {
    uint32_t packedData;
    uint32_t offsetID;
};

struct ChunkPositionHash {
    std::size_t operator()(const glm::ivec3& k) const {
        std::size_t seed = 0;
        // Hash combine pattern from Boost
        seed ^= std::hash<int>()(k.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>()(k.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>()(k.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

class Chunk;

typedef std::unordered_map<glm::ivec3, Chunk*, ChunkPositionHash> ChunkMap;

class Chunk {
    private:
        uint8_t voxelIDs[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        std::vector<Voxel::PackedVoxel> meshData;
        bool shouldRenderFace(int x, int y, int z, int d, int direction) const;
        void greedyMeshAxis(std::vector<Voxel::PackedVoxel>& meshData, int d, int u, int v, int direction, Voxel::VoxelFace faceDir);
    public:
        glm::ivec3 chunkPosition; // relative to chunks
        ChunkMap* map_ptr;
        bool hasTriggeredMeshUpdate;

        Chunk(glm::ivec3 position, ChunkMap* map = nullptr);
        void setVoxel(int x, int y, int z, uint8_t voxelID);
        uint8_t getVoxel_ID(int x, int y, int z) const;
        uint8_t getAdjChunkVoxel_ID(Voxel::VoxelFace adjChunkDir, int x, int y, int z) const;
        std::vector<Voxel::PackedVoxel> createMeshData();
};