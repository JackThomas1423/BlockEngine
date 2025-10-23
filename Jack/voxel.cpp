// Representation of voxel:
//  - multi dimensional array stores posistion
//  - color stored as an id for a look up table (prob done in shader)
//  - 
// Required functions:
//  - Voxel chunk mesh former
//  - object rendering
//  - 

#include "voxel.hpp"

Mesh voxelToMesh() {
    return {
        0.5f,  0.5f, 0.5f, 0.0f,    // top right          [0]
        0.5f, -0.5f, 0.5f, 1.0f,    // bottom right       [1]
        -0.5f, -0.5f, 0.5f, 2.0f,   // bottom left        [2]
        -0.5f,  0.5f, 0.5f, 3.0f,   // top left           [3]

        0.5f,  0.5f, -0.5f, 0.0f,   // back top right     [4]
        0.5f, -0.5f, -0.5f, 1.0f,   // back bottom right  [5]
        -0.5f, -0.5f, -0.5f, 2.0f,  // back bottom left   [6]
        -0.5f,  0.5f, -0.5f, 3.0f   // back top left      [7]
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
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_WIDTH; ++z) {
                ColorId color_id = chunk.data[x][y][z];
                // zero means air block (skip it)
                if (color_id == 0) continue;
                Mesh cubeModel = voxelToMesh();
                //cube model needs to add global_position and xyz
                chunkMesh.insert(chunkMesh.end(), cubeModel.begin(), cubeModel.end());
            }
        }
    }
    return chunkMesh;
}

Mesh voxelPlain() {
    Mesh plain;
    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            Mesh instance = voxelToMesh();
            for (int index = 0; index < 8; ++index) {
                instance[index*4] += x;
                instance[((index*4)+1)] += z;
            }

            plain.insert(std::end(plain), std::begin(instance), std::end(instance));
        }
    }
    return plain;
}

std::vector<unsigned int> genConnectors() {
    std::vector<unsigned int> connections;
    for (int cubeIndex = 0; cubeIndex < 100; ++cubeIndex) {
        auto cc = cubeConnector();
        for (int n = 0; n < 36; ++n) {
            cc[n] += 8*cubeIndex;
        }
        connections.insert(std::end(connections), std::begin(cc), std::end(cc));
    }

    return connections;
}