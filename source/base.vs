#version 410 core
layout (location = 0) in uint packedData;
layout (location = 1) in uint packedChunkPosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
  int length;
  int height;
  int face;
  int color;
  int lod;
  vec3 FragPos;
  vec3 Normal;
} vs_out;

// Optimized bit layout:
// x: 4 bits (0-15)         - bits 0-3
// y: 4 bits (0-15)         - bits 4-7
// z: 4 bits (0-15)         - bits 8-11
// length: 4 bits (1-16)    - bits 12-15  (stored as 0-15, add 1 when unpacking)
// height: 4 bits (1-16)    - bits 16-19  (stored as 0-15, add 1 when unpacking)
// color: 8 bits (0-255)    - bits 20-27
// facing: 3 bits (0-7)     - bits 28-30

#define GET_X(packed)        (((packed) >> 0u) & 0xFu)
#define GET_Y(packed)        (((packed) >> 4u) & 0xFu)
#define GET_Z(packed)        (((packed) >> 8u) & 0xFu)
#define GET_LENGTH(packed)   ((((packed) >> 12u) & 0xFu) + 1u)  // Add 1 to get 1-16 range
#define GET_HEIGHT(packed)   ((((packed) >> 16u) & 0xFu) + 1u)  // Add 1 to get 1-16 range
#define GET_COLOR(packed)    (((packed) >> 20u) & 0xFFu)
#define GET_FACING(packed)   (((packed) >> 28u) & 0x7u)

#define GET_CHUNK_X(packed)   (((packed) >> 0u) & 0xFFu) - 128
#define GET_CHUNK_Y(packed)   (((packed) >> 8u) & 0xFFu) - 128
#define GET_CHUNK_Z(packed)   (((packed) >> 16u) & 0xFFu) - 128
#define GET_CHUNK_LOD(packed) (((packed) >> 24u) & 0xFFu) - 128

/*
// Left Right might need to be swapped idk
const vec3 normalConvert[6] = vec3[6](
    vec3(0.0, 0.0, -1.0), // Front
    vec3(0.0, 0.0, 1.0), // Back
    vec3(0.0, 1.0, 0.0), // Top
    vec3(0.0, -1.0, 0.0), // Bottom
    vec3(-1.0, 0.0, 0.0), // Left
    vec3(1.0, 0.0, 0.0)  // Right
);*/
const vec3 normalConvert[6] = vec3[6](
    vec3(0.0, 0.0, 1.0), // Front
    vec3(0.0, 0.0, -1.0), // Back
    vec3(0.0, 1.0, 0.0), // Top
    vec3(0.0, -1.0, 0.0), // Bottom
    vec3(-1.0, 0.0, 0.0), // Left
    vec3(1.0, 0.0, 0.0)  // Right
);

void main() {
    
    int x = int(GET_X(packedData));
    int y = int(GET_Y(packedData));
    int z = int(GET_Z(packedData));
    int length = int(GET_LENGTH(packedData));  // Now 1-16
    int height = int(GET_HEIGHT(packedData));  // Now 1-16
    int color = int(GET_COLOR(packedData));
    int face = int(GET_FACING(packedData));

    ivec3 chunkOffset = ivec3(GET_CHUNK_X(packedChunkPosition), GET_CHUNK_Y(packedChunkPosition), GET_CHUNK_Z(packedChunkPosition)) * ivec3(16,16,16);
    vec3 worldPosition = vec3(x, y, z) + chunkOffset;
    vs_out.FragPos = vec3(model * vec4(worldPosition, 1.0));
    
    //gl_position was here
    vs_out.color = color;
    vs_out.face = face;
    vs_out.length = length;
    vs_out.height = height;
    vs_out.lod = int(GET_CHUNK_LOD(packedChunkPosition));
    vs_out.Normal = mat3(transpose(inverse(model))) * normalConvert[face];
    gl_Position = vec4(worldPosition, 1.0); 

}