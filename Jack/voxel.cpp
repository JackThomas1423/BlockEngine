// Representation of voxel:
//  - multi dimensional array stores posistion
//  - color stored as an id for a look up table (prob done in shader)
//  - 
// Required functions:
//  - Voxel chunk mesh former
//  - object rendering
//  - 

#include "voxel.hpp"

std::vector<float> voxelToMesh(float x, float y, float z, float c = 0.0f) {
    return {
        0.5f + x,  0.5f + y, 0.5f + z, c,    // top right          [0]
        0.5f + x, -0.5f + y, 0.5f + z, c,    // bottom right       [1]
        -0.5f + x, -0.5f + y, 0.5f + z, c,   // bottom left        [2]
        -0.5f + x,  0.5f + y, 0.5f + z, c,   // top left           [3]

        0.5f + x,  0.5f + y, -0.5f + z, c,   // back top right     [4]
        0.5f + x, -0.5f + y, -0.5f + z, c,   // back bottom right  [5]
        -0.5f + x, -0.5f + y, -0.5f + z, c,  // back bottom left   [6]
        -0.5f + x,  0.5f + y, -0.5f + z, c   // back top left      [7]
    };
}

std::vector<unsigned int> cubeConnector() {
    return {
        0, 1, 3,   // front face
        1, 2, 3,

        4, 5, 7,   // back face
        5, 6, 7,

        4, 0, 7,   // right face
        0, 3, 7,

        5, 1, 6,   // left face
        1, 2, 6,

        1, 5, 0,   // top face
        5, 4, 0,

        2, 6, 3,   // bottom face
        6, 7, 3
    };
}

// mesh the chunk with a mesh to get the mesh using the chunk mesh method implemented in this mesh chunk function (mesh)
Mesh meshChunk(Chunk chunk) {
    Mesh chunkMesh; // the chunk mesh
    int totalCubes = 0;
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                ColorId color_id = chunk.data[x][y][z];
                // zero means air block (skip it)
                if (color_id != 0) {
                    std::vector<float> cubeModel = voxelToMesh((float)x,(float)y,(float)z,(float)color_id);
                    chunkMesh.vertices.insert(chunkMesh.vertices.end(), cubeModel.begin(), cubeModel.end());
                    totalCubes += 1;
                } else {continue;}
            }
        }
    }
    for (int cubeIndex = 0; cubeIndex < totalCubes; ++cubeIndex) {
        auto cc = cubeConnector();
        for (int n = 0; n < 36; ++n) {
            cc[n] += 8*cubeIndex;
        }
        chunkMesh.indices.insert(std::end(chunkMesh.indices), std::begin(cc), std::end(cc));
    }
    return chunkMesh;
}