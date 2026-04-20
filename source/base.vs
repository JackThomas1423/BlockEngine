#version 410 core
layout (location = 0) in uint vertexData;    // Per-vertex: local pos, dimensions, color, face
layout (location = 1) in vec2 aTexCoord;

//uniform sampler2D testTexture;

uniform uint instanceData;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

flat out int fsColor;
//flat out vec3 Normal;
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
// Vertex data unpacking (32-bit)
#define GET_X(data) (((data) >> 0u) & 0xFu)
#define GET_Y(data) (((data) >> 4u) & 0xFu)
#define GET_Z(data) (((data) >> 8u) & 0xFu)
#define GET_LENGTH(data) ((((data) >> 12u) & 0xFu) + 1u)
#define GET_HEIGHT(data) ((((data) >> 16u) & 0xFu) + 1u)
#define GET_COLOR(data) (((data) >> 20u) & 0xFFu)
#define GET_FACING(data) (((data) >> 28u) & 0x7u)

// Instance data unpacking (32-bit)
#define GET_CHUNK_X(data) ((((data) >> 0u) & 0x3FFu) - 512u)
#define GET_CHUNK_Y(data) ((((data) >> 10u) & 0x3FFu) - 512u)
#define GET_CHUNK_Z(data) ((((data) >> 20u) & 0xFFFu) - 2048u)

const vec3 cubeFaces[24] = vec3[24](
    // Front Face (Z = 0.5)
    vec3(-0.5, 0.5, 0.5), vec3(-0.5, -0.5, 0.5),
    vec3(0.5, 0.5, 0.5), vec3(0.5, -0.5, 0.5),
    // Back Face (Z = -0.5)
    vec3(0.5, 0.5, -0.5), vec3(0.5, -0.5, -0.5),
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, -0.5, -0.5),
    // Top Face (Y = 0.5)
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, 0.5, 0.5),
    vec3(0.5, 0.5, -0.5), vec3(0.5, 0.5, 0.5),
    // Bottom Face (Y = -0.5)
    vec3(-0.5, -0.5, 0.5), vec3(-0.5, -0.5, -0.5),
    vec3(0.5, -0.5, 0.5), vec3(0.5, -0.5, -0.5),
    // Right Face (X = 0.5)
    vec3(0.5, 0.5, 0.5), vec3(0.5, -0.5, 0.5),
    vec3(0.5, 0.5, -0.5), vec3(0.5, -0.5, -0.5),
    // Left Face (X = -0.5)
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, -0.5, -0.5),
    vec3(-0.5, 0.5, 0.5), vec3(-0.5, -0.5, 0.5)
);

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
const vec2 textureConvert[4] = vec2[4](
    vec2(0.0, 1.0),  // top-left
    vec2(0.0, 0.0),  // bottom-left
    vec2(1.0, 1.0),  // top-right
    vec2(1.0, 0.0)   // bottom-right
)
const vec3 normalConvert[6] = vec3[6](
    vec3(0.0, 0.0, 1.0), // Front
    vec3(0.0, 0.0, -1.0), // Back
    vec3(0.0, 1.0, 0.0), // Top
    vec3(0.0, -1.0, 0.0), // Bottom
    vec3(-1.0, 0.0, 0.0), // Left
    vec3(1.0, 0.0, 0.0)  // Right
);


void main() {
    // Unpack vertex data
    int x      = int(GET_X(vertexData));
    int y      = int(GET_Y(vertexData));
    int z      = int(GET_Z(vertexData));
    int length = int(GET_LENGTH(vertexData));
    int height = int(GET_HEIGHT(vertexData));
    int color  = int(GET_COLOR(vertexData));
    int face   = int(GET_FACING(vertexData));

    //vec4 sample = texture(testTexture, aTexCoord)
    
    // Unpack instance data
    int chunkX = int(GET_CHUNK_X(instanceData));
    int chunkY = int(GET_CHUNK_Y(instanceData));
    int chunkZ = int(GET_CHUNK_Z(instanceData));

    vec3 voxelPosition = vec3(x, y, z);
    vec3 worldPosition = voxelPosition + (ivec3(chunkX, chunkY, chunkZ) * ivec3(16, 16, 16));

    float lengthV = float(length);
    float heightU = float(height);
    
    vec3 scale = vec3(1.0);
    
    // Face ordering: FRONT=0, BACK=1, TOP=2, BOTTOM=3, RIGHT=4, LEFT=5
    // For each face: d=depth axis, u=height axis, v=length axis
    if (face == 0 || face == 1) {
        // FRONT/BACK: d=2(Z), u=0(X), v=1(Y)
        // height expands in u(X), length expands in v(Y)
        scale = vec3(heightU, lengthV, 1.0);
        fsColor = 1;
    } else if (face == 2 || face == 3) {
        // TOP/BOTTOM: d=1(Y), u=0(X), v=2(Z)
        // height expands in u(X), length expands in v(Z)
        scale = vec3(heightU, 1.0, lengthV);
        fsColor = 2;
    } else {
        // LEFT/RIGHT: d=0(X), u=1(Y), v=2(Z)
        // height expands in u(Y), length expands in v(Z)
        scale = vec3(1.0, heightU, lengthV);
        fsColor = 3;
    }

    int index = face * 4;
    int vertexIndex = gl_VertexID % 4;
    vec3 localVertex = cubeFaces[index + vertexIndex] + 0.5;
    
    vec3 worldVertex = worldPosition + localVertex * scale;

    mat4 pv = projection * view * model;
    FragPos = vec3(model * vec4(worldVertex, 1.0));
    Normal = mat3(transpose(inverse(model))) * normalConvert[face];
    gl_Position = pv * vec4(worldVertex, 1.0);
    TexCoords = aTexCoords;
}