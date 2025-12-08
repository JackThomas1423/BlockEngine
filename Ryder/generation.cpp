#include "generation.hpp"
#include <cstdint>
#include <cstdlib>

void genChunk(Chunk& chunk) {
    glm::ivec3 pos = chunk.global_position;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                float height_value = static_cast<int>(((glm::perlin(glm::vec2(x + pos.x * CHUNK_WIDTH, z + pos.z * CHUNK_DEPTH) * 0.1f) + 1.0f) / 2.0f) * MAX_HEIGHT);
                if (y + pos.y * CHUNK_HEIGHT <= height_value) {
                    chunk.data[x][y][z] = 2; // solid block
                } else {
                    chunk.data[x][y][z] = 0; // air
                }
            }
        }
    }
}