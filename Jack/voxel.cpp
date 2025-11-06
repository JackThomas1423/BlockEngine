#include "voxel.hpp"
std::vector<unsigned int> frontFace = { 0, 2, 1, 0, 3, 2 }; // z+
std::vector<unsigned int> backFace = { 4, 6, 7, 4, 5, 6 }; // z-
std::vector<unsigned int> leftFace = { 3, 6, 2, 3, 7, 6 }; // x-
std::vector<unsigned int> rightFace = { 0, 5, 4, 0, 1, 5 }; // x+
std::vector<unsigned int> topFace = { 3, 4, 7, 3, 0, 4 }; // y+
std::vector<unsigned int> bottomFace = { 2, 5, 1, 2, 6, 5 }; // y-

std::vector<float> voxelToMesh(float x, float y, float z, float c = 0.0f) {
    return { 0.5f + x, 0.5f + y, 0.5f + z, c, // top right [0]
    0.5f + x, -0.5f + y, 0.5f + z, c, // bottom right [1]
    -0.5f + x, -0.5f + y, 0.5f + z, c, // bottom left [2]
    -0.5f + x, 0.5f + y, 0.5f + z, c, // top left [3]
    0.5f + x, 0.5f + y, -0.5f + z, c, // back top right [4]
    0.5f + x, -0.5f + y, -0.5f + z, c, // back bottom right [5]
    -0.5f + x, -0.5f + y, -0.5f + z, c, // back bottom left [6]
    -0.5f + x, 0.5f + y, -0.5f + z, c // back top left [7]
    };
}

Mesh meshChunk(Chunk chunk) {
    Mesh chunkMesh;
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                ColorId color_id = chunk.data[x][y][z];
                if (color_id == 0) continue;
                std::vector<float> cubeVerts = voxelToMesh((float)x, (float)y, (float)z, (float)color_id);
                unsigned int vertexStartIndex = chunkMesh.vertices.size() / 4;
                chunkMesh.vertices.insert(chunkMesh.vertices.end(), cubeVerts.begin(), cubeVerts.end());
                auto isAir = [&](int nx, int ny, int nz) {
                    if (nx < 0 || ny < 0 || nz < 0 || nx >= CHUNK_WIDTH || ny >= CHUNK_HEIGHT || nz >= CHUNK_DEPTH) return true;
                    return chunk.data[nx][ny][nz] == 0;
                };
                if (isAir(x, y, z + 1)) // front
                    for (auto id : frontFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
                if (isAir(x, y, z - 1)) // back
                    for (auto id : backFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
                if (isAir(x - 1, y, z)) // left
                    for (auto id : leftFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
                if (isAir(x + 1, y, z)) // right
                    for (auto id : rightFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
                if (isAir(x, y + 1, z)) // top
                    for (auto id : topFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
                if (isAir(x, y - 1, z)) // bottom
                    for (auto id : bottomFace)
                        chunkMesh.indices.push_back(vertexStartIndex + id);
            }
        }
    }
    return chunkMesh;
}