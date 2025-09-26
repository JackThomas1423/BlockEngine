#include <cstdint>
#include <vector>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

// uint8_t is the type used for the colors. this maybe turned into a struct if we need more voxel specific data is needed
// add chunk specific data as needed

typedef uint8_t ColorId;

struct Chunk {
    ColorId data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
};

std::vector<float> voxelToMesh();