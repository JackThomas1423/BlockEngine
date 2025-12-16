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

#define MAX_HEIGHT CHUNK_HEIGHT * 1

struct ChunkVertex {
    uint32_t packedData;
    uint32_t offsetID;
};

class Chunk {
    private:
        uint8_t voxelIDs[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        std::vector<Voxel::PackedVoxel> meshData;
        bool isFaceVisible(int x, int y, int z, int dx, int dy, int dz) const;
        bool shouldRenderFace(int x, int y, int z, int d, int direction) const;
        void greedyMeshAxis(std::vector<Voxel::PackedVoxel>& meshData, int d, int u, int v, int direction, Voxel::VoxelFace faceDir);
    public:
        glm::ivec3 chunkPosition; // relative to chunks
        bool hasTriggeredMeshUpdate;

        Chunk(glm::ivec3 position);
        void setVoxel(int x, int y, int z, uint8_t voxelID);
        uint8_t getVoxel_ID(int x, int y, int z) const;
        std::vector<Voxel::PackedVoxel> createMeshData();
};