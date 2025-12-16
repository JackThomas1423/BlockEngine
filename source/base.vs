#version 410 core
layout (location = 0) in uint packedData;
layout (location = 1) in uint offsetID;

layout(std140) uniform ChunkOffsets {
  ivec4 offsets[100];
};

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
  int length;
  int height;
  int face;
  int color;
} vs_out;

#define GET_X(packed) (((packed) >> 0u) & 0xFu)
#define GET_Y(packed) (((packed) >> 4u) & 0xFu)
#define GET_Z(packed) (((packed) >> 8u) & 0xFu)
#define GET_LENGTH(packed) (((packed) >> 12u) & 0xFu)
#define GET_HEIGHT(packed) (((packed) >> 16u) & 0xFu)
#define GET_COLOR(packed) (((packed) >> 20u) & 0xFu)
#define GET_FACING(packed) (((packed) >> 24u) & 0xFFu)

void main() {
    int x = int(GET_X(packedData));
    int y = int(GET_Y(packedData));
    int z = int(GET_Z(packedData));
    int length = int(GET_LENGTH(packedData));
    int height = int(GET_HEIGHT(packedData));
    int color = int(GET_COLOR(packedData));
    int face = int(GET_FACING(packedData));

    vec3 voxelPosition = vec3(x, y, z);
    vec3 worldPosition = voxelPosition + (offsets[offsetID].xyz * ivec3(16, 16, 16));

    gl_Position = model * vec4(worldPosition, 1.0);
    vs_out.color = color;
    vs_out.face = face;
    vs_out.length = length;
    vs_out.height = height;
}