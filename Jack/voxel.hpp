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

inline PackedVoxel packVertexData(int x, int y, int z, int length, int height, int colorIndex, int facing) {
    // Mask each value to appropriate bit count
    PackedVoxel packed = 0;
    packed |= (x & 0xF) << 0;           // bits 0-3
    packed |= (y & 0xF) << 4;           // bits 4-7
    packed |= (z & 0xF) << 8;           // bits 8-11
    packed |= (length & 0xF) << 12;     // bits 12-15
    packed |= (height & 0xF) << 16;     // bits 16-19
    packed |= (colorIndex & 0xF) << 20; // bits 20-23 (4 bits for color, 16 colors)
    packed |= (facing & 0xF) << 24;    // bits 24-31 (8 bits for facing)
    
    return packed;
}

}