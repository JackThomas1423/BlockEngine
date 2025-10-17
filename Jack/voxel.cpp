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
        0.5f,  0.5f, 0.5f,    // top right          [0]
        0.5f, -0.5f, 0.5f,    // bottom right       [1]
        -0.5f, -0.5f, 0.5f,   // bottom left        [2]
        -0.5f,  0.5f, 0.5f,   // top left           [3]

        0.5f,  0.5f, -0.5f,   // back top right     [4]
        0.5f, -0.5f, -0.5f,   // back bottom right  [5]
        -0.5f, -0.5f, -0.5f,  // back bottom left   [6]
        -0.5f,  0.5f, -0.5f,  // back top left      [7]
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

Mesh voxelPlain() {
    Mesh plain;
    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            Mesh instance = voxelToMesh();
            for (int n = 0; n < 7; ++n) { instance[n*3] += x; }
            for (int n = 0; n < 7; ++n) { instance[(n*3)+2] += z; }
            plain.insert(std::end(plain), std::begin(instance), std::end(instance));
        }
    }
    return plain;
}

std::vector<unsigned int> genConnectors() {
    std::vector<unsigned int> connections;
    auto cc = cubeConnector();
    connections.insert(std::end(connections), std::begin(cc), std::end(cc));
    for (int n = 0; n < 36; ++n) {
        cc[n] += 8;
    }
    connections.insert(std::end(connections), std::begin(cc), std::end(cc));
    return connections;
}