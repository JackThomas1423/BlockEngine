#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Voxel
{

// Vertex data: local position, dimensions, color, and face
// 32 bits total
typedef uint32_t PackedVoxel;

// Instance data: chunk position
// 32 bits total
typedef uint32_t PackedChunkData;

enum VoxelFace : uint8_t {
    FRONT  = 0x00,
    BACK   = 0x01,
    TOP    = 0x02,
    BOTTOM = 0x03,
    RIGHT  = 0x04,
    LEFT   = 0x05
};

const glm::vec3 FaceNormals[6] = {
    glm::vec3( 0.0f,  0.0f,  1.0f),
    glm::vec3( 0.0f,  0.0f, -1.0f),
    glm::vec3( 0.0f,  1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f,  0.0f),
    glm::vec3( 1.0f,  0.0f,  0.0f),
    glm::vec3(-1.0f,  0.0f,  0.0f)
};

enum VoxelColor : uint8_t {
    EMPTY   = 0, // reserved value for "no voxel"
    RED     = 1,
    GREEN   = 2,
    BLUE    = 3,
    YELLOW  = 4,
    MAGENTA = 5,
    CYAN    = 6,
    WHITE   = 7,
    BLACK   = 8
};

// 32-bit vertex layout (per-vertex data):
// Local voxel x: 4 bits (0-15)      - bits 0-3
// Local voxel y: 4 bits (0-15)      - bits 4-7
// Local voxel z: 4 bits (0-15)      - bits 8-11
// length: 4 bits (1-16)             - bits 12-15
// height: 4 bits (1-16)             - bits 16-19
// color: 8 bits (0-255)             - bits 20-27
// facing: 3 bits (0-7)              - bits 28-30
// (1 bit unused)                    - bit 31

inline PackedVoxel packVertexData(int localX, int localY, int localZ,
                                   int length, int height,
                                   int colorIndex, int facing) {
    uint32_t packed = 0;

    packed |= (localX & 0xF) << 0;          // bits 0-3
    packed |= (localY & 0xF) << 4;          // bits 4-7
    packed |= (localZ & 0xF) << 8;          // bits 8-11
    packed |= ((length - 1) & 0xF) << 12;   // bits 12-15
    packed |= ((height - 1) & 0xF) << 16;   // bits 16-19
    packed |= (colorIndex & 0xFF) << 20;    // bits 20-27
    packed |= (facing & 0x7) << 28;         // bits 28-30

    return packed;
}

// 32-bit instance layout (per-chunk data):
// Chunk X: 10 bits (0-1023)         - bits 0-9   (supports -512 to +511 with offset)
// Chunk Y: 10 bits (0-1023)         - bits 10-19 (supports -512 to +511 with offset)
// Chunk Z: 12 bits (0-4095)         - bits 20-31 (supports -2048 to +2047 with offset)

inline PackedChunkData packChunkData(int chunkX, int chunkY, int chunkZ) {
    uint32_t packed = 0;

    packed |= ((chunkX + 512) & 0x3FF) << 0;    // bits 0-9
    packed |= ((chunkY + 512) & 0x3FF) << 10;   // bits 10-19
    packed |= ((chunkZ + 2048) & 0xFFF) << 20;  // bits 20-31

    return packed;
}

}
