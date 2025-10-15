// Representation of voxel:
//  - multi dimensional array stores posistion
//  - color stored as an id for a look up table (prob done in shader)
//  - 
// Required functions:
//  - Voxel chunk mesh former
//  - object rendering
//  - 

#include "voxel.hpp"

std::vector<float> voxelToMesh(ColorId cid) {
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