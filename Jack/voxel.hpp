#pragma once

#include <cstdint>
#include <algorithm>
#include <vector>
#include <array>
#include <iostream>
#include <unordered_map>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>


#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 32
#define CHUNK_DEPTH 32

// uint8_t is the type used for the colors. this maybe turned into a struct if we need more voxel specific data is needed
// add chunk specific data as needed

// color id of zero should represent air/no-block
typedef uint8_t ColorId;

struct Vertex {
    glm::vec3 position;
    ColorId color;

    Vertex(const glm::vec3& pos, ColorId col) : position(pos), color(col) {}

    bool operator==(const Vertex& other) const {
        return position == other.position && color == other.color;
    }
};

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct VertexHash {
    std::size_t operator()(const Vertex& v) const noexcept {
        std::size_t hx = std::hash<int>()(v.position.x);
        std::size_t hy = std::hash<int>()(v.position.y);
        std::size_t hz = std::hash<int>()(v.position.z);
        std::size_t c = std::hash<int>()(v.color);

        // Combine the three hashes
        std::size_t seed = hx;
        seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hz + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= c + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};


class Chunk {
private:
    std::unordered_map<Vertex, unsigned int, VertexHash> vertexMap;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int addVertex(const Vertex& vertex);
    void addFace(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);
public:
    glm::vec3 global_position;
    ColorId data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

    Chunk(glm::vec3 position);
    Mesh computeMesh();
};