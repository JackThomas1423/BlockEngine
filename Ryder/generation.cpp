#include "generation.hpp"
#include <cstdint>
#include <cstdlib>

// Global seed and offsets for noise
static int g_noise_seed = 0;
static float g_seedOffsetX = 0.0f;
static float g_seedOffsetY = 0.0f;
static float g_seedOffsetZ = 0.0f;

void setNoiseSeed(uint32_t seed) {
    g_noise_seed = seed;
    // simple scrambling using an LCG to produce two deterministic offsets
    uint32_t s = static_cast<uint32_t>(seed);
    s = s * 1664525u + 1013904223u;
    uint32_t s2 = s * 1664525u + 1013904223u;
    // scale down to a reasonable offset in noise space
    g_seedOffsetX = (float)(s % 100000u) * 0.0001f; // 0 .. ~10.0
    g_seedOffsetY = (float)((s / 100000u) % 100000u) * 0.0001f;
    g_seedOffsetZ = (float)(s2 % 100000u) * 0.0001f;
}

std::vector<float> noise(int baseX, int baseY, int baseZ, float scale = 0.08f) {
    // Sample Perlin at world coordinates so adjacent chunks produce continuous noise
    std::vector<float> result;
    result.reserve(CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT);
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                float wx = (float)(baseX + x) * scale + g_seedOffsetX; // world x in noise space
                float wy = (float)(baseY + y) * scale + g_seedOffsetY; // world y in noise space
                float wz = (float)(baseZ + z) * scale + g_seedOffsetZ; // world z in noise space
                float n = glm::perlin(glm::vec3(wx, wy, wz));
                result.push_back(n);
            }
        }
    }
    return result;
}

void genChunk(Chunk& chunk) {
    // Generate a heightmap using glm::perlin sampled in world space
    int baseX = static_cast<int>(std::floor(chunk.global_position.x));
    int baseY = static_cast<int>(std::floor(chunk.global_position.y));
    int baseZ = static_cast<int>(std::floor(chunk.global_position.z));
    std::vector<float> n = noise(baseX, baseY, baseZ, 0.12f);
    // safety: ensure returned vector has expected size
    if ((int)n.size() < CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT) return;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for(int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                int maxHeight = static_cast<int>(n[x * CHUNK_DEPTH + z] * (CHUNK_HEIGHT - 1));
                if (maxHeight < 0) maxHeight = 0;
                if (maxHeight >= CHUNK_HEIGHT) maxHeight = CHUNK_HEIGHT - 1;

                for (int y = 0; y <= maxHeight; ++y) {
                    chunk.data[x][y][z] = 2; // set block to solid
                }
            }
        }
    }
}