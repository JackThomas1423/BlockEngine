#include "generation.hpp"

std::vector<float> noise(int width, int height, float scale = 8.0f) {
    std::vector<float> result;
    result.reserve(width * height);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            float nx = (float)x / (float)width;
            float ny = (float)y / (float)height;
            float n = glm::perlin(glm::vec2(nx * scale, ny * scale));
            result.push_back(n);
        }
    }
    return result;
}

void genChunk(Chunk& chunk) {
    // Generate a heightmap using glm::perlin; ensure normalization is correct and clamp results
    std::vector<float> n = noise(CHUNK_WIDTH, CHUNK_DEPTH, 1.0f);
    // safety: ensure returned vector has expected size
    if ((int)n.size() < CHUNK_WIDTH * CHUNK_DEPTH) return;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            // glm::perlin returns approximately in [-1,1]. Normalize properly to [0,1].
            float sample = n[x * CHUNK_DEPTH + z];
            float heightValue = (sample + 1.0f) / 2.0f; // correct normalization
            // clamp to [0,1] to avoid any numerical overshoot
            if (heightValue < 0.0f) heightValue = 0.0f;
            if (heightValue > 1.0f) heightValue = 1.0f;

            int maxHeight = static_cast<int>(heightValue * (CHUNK_HEIGHT - 1));
            if (maxHeight < 0) maxHeight = 0;
            if (maxHeight >= CHUNK_HEIGHT) maxHeight = CHUNK_HEIGHT - 1;

            for (int y = 0; y <= maxHeight; ++y) {
                chunk.data[x][y][z] = 2; // set block to solid
            }
        }
    }
}