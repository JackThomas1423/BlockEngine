#pragma once

#include <cstdint>
#include <algorithm>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

// uint8_t is the type used for the colors. this maybe turned into a struct if we need more voxel specific data is needed
// add chunk specific data as needed

// color id of zero should represent air/no-block
typedef uint8_t ColorId;
typedef glm::vec3 Vertex;

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct VertexHash {
    std::size_t operator()(const glm::vec3& v) const noexcept {
        std::size_t hx = std::hash<int>()(v.x);
        std::size_t hy = std::hash<int>()(v.y);
        std::size_t hz = std::hash<int>()(v.z);

        // Combine the three hashes
        std::size_t seed = hx;
        seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hz + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};


class Chunk {
public:
    glm::vec3 global_position;
    ColorId data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
    Chunk(glm::vec3 position) {
        global_position = position;
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_DEPTH; ++z) {
                    data[x][y][z] = 0;
                }
            }
        }
    }
};

Mesh meshChunk(Chunk chunk);