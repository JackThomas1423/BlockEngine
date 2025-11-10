#include "voxel.hpp"

std::vector<unsigned int> frontFace = { 0, 2, 1, 0, 3, 2 }; // z+
std::vector<unsigned int> backFace = { 4, 6, 7, 4, 5, 6 }; // z-
std::vector<unsigned int> leftFace = { 3, 6, 2, 3, 7, 6 }; // x-
std::vector<unsigned int> rightFace = { 0, 5, 4, 0, 1, 5 }; // x+
std::vector<unsigned int> topFace = { 3, 4, 7, 3, 0, 4 }; // y+
std::vector<unsigned int> bottomFace = { 2, 5, 1, 2, 6, 5 }; // y-

std::vector<std::vector<unsigned int>> voxelFaces = {
    rightFace,
    leftFace,
    topFace,
    bottomFace,
    frontFace,
    backFace
};

std::vector<std::array<int, 3>> voxelNeighbors = {
    { 1, 0, 0 },
    { -1, 0, 0 },
    { 0, 1, 0 },
    { 0, -1, 0 },
    { 0, 0, 1 },
    { 0, 0, -1 }
};

std::vector<float> voxelToMesh(float x, float y, float z, float c = 0.0f) {
    return { 0.5f + x, 0.5f + y, 0.5f + z, c, // top right [0]
    0.5f + x, -0.5f + y, 0.5f + z, c,         // bottom right [1]
    -0.5f + x, -0.5f + y, 0.5f + z, c,        // bottom left [2]
    -0.5f + x, 0.5f + y, 0.5f + z, c,         // top left [3]
    0.5f + x, 0.5f + y, -0.5f + z, c,         // back top right [4]
    0.5f + x, -0.5f + y, -0.5f + z, c,        // back bottom right [5]
    -0.5f + x, -0.5f + y, -0.5f + z, c,       // back bottom left [6]
    -0.5f + x, 0.5f + y, -0.5f + z, c         // back top left [7]
    };
}

Mesh meshChunk(Chunk chunk) {
    Mesh chunkMesh;

    std::unordered_map<Vertex, int, VertexHash> vertexMap;
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                ColorId color_id = chunk.data[x][y][z];
                
                if (color_id == 0) continue;
                
                // generate vertices
                std::vector<float> cubeVerts = voxelToMesh((float)x, (float)y, (float)z, (float)color_id);

                for (int i = 0; i < cubeVerts.size() / 4; ++i) {
                    // Adjust vertex positions based on chunk's global position
                    cubeVerts[i * 4 + 0] += chunk.global_position.x;
                    cubeVerts[i * 4 + 1] += chunk.global_position.y;
                    cubeVerts[i * 4 + 2] += chunk.global_position.z;
                }

                chunkMesh.vertices.insert(chunkMesh.vertices.end(), cubeVerts.begin(), cubeVerts.end());

                // generate indices
                unsigned int vertexOffset = (unsigned int)(chunkMesh.vertices.size() / 4 - 8);
                for (int face = 0; face < 6; ++face) {
                    std::array<int, 3> neighborOffset = voxelNeighbors[face];
                    int neighborX = x + neighborOffset[0];
                    int neighborY = y + neighborOffset[1];
                    int neighborZ = z + neighborOffset[2];

                    bool isFaceVisible = false;

                    // Check if neighbor is out of bounds
                    if (neighborX < 0 || neighborX >= CHUNK_WIDTH ||
                        neighborY < 0 || neighborY >= CHUNK_HEIGHT ||
                        neighborZ < 0 || neighborZ >= CHUNK_DEPTH) {
                        isFaceVisible = true; // Out of bounds, face is visible
                    } else {
                        // Check if neighbor voxel is empty
                        if (chunk.data[neighborX][neighborY][neighborZ] == 0) {
                            isFaceVisible = true; // Neighbor is empty, face is visible
                        }
                    }

                    if (isFaceVisible) {
                        for (unsigned int index : voxelFaces[face]) {
                            chunkMesh.indices.push_back(vertexOffset + index);
                        }
                    }
                }
            }
        }
    }
    return chunkMesh;
}