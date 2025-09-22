#include <cstdint>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

//uint8_t is the type used for the colors. this maybe turned into a struct if we need more voxel spesific data is needed
//add chunk spesific data as needed
struct Chunk {
    uint8_t data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
};