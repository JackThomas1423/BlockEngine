#pragma once

#include <cstdint>

namespace Voxel
{

typedef uint32_t PackedVoxel;

enum VoxelFace : uint8_t {
    FRONT  = 0x00,
    BACK   = 0x01,
    TOP    = 0x02,
    BOTTOM = 0x03,
    RIGHT  = 0x04,
    LEFT   = 0x05
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

// Optimized bit layout (32 bits total):
// x: 4 bits (0-15)         - bits 0-3
// y: 4 bits (0-15)         - bits 4-7
// z: 4 bits (0-15)         - bits 8-11
// length: 4 bits (1-16)    - bits 12-15  <- Represents 1-16, not 0-15!
// height: 4 bits (1-16)    - bits 16-19  <- Represents 1-16, not 0-15!
// color: 8 bits (0-255)    - bits 20-27  <- 256 possible colors!
// facing: 3 bits (0-7)     - bits 28-30
// (1 bit unused)           - bit 31

inline PackedVoxel packVertexData(int x, int y, int z, int length, int height, int colorIndex, int facing) {
    PackedVoxel packed = 0;
    packed |= (x & 0xF) << 0;                // bits 0-3   (4 bits)
    packed |= (y & 0xF) << 4;                // bits 4-7   (4 bits)
    packed |= (z & 0xF) << 8;                // bits 8-11  (4 bits)
    packed |= ((length - 1) & 0xF) << 12;    // bits 12-15 (4 bits) - subtract 1 to store 1-16 as 0-15
    packed |= ((height - 1) & 0xF) << 16;    // bits 16-19 (4 bits) - subtract 1 to store 1-16 as 0-15
    packed |= (colorIndex & 0xFF) << 20;     // bits 20-27 (8 bits) - 256 colors!
    packed |= (facing & 0x7) << 28;          // bits 28-30 (3 bits)
    
    return packed;
}

}