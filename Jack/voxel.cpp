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
        1.0f, 1.0f, 1.0f, (float)cid,
        1.0f, 1.0f, 0.0f, (float)cid,
        1.0f, 0.0f, 0.0f, (float)cid,
        1.0f, 0.0f, 1.0f, (float)cid,

        0.0f, 1.0f, 1.0f, (float)cid,
        0.0f, 0.0f, 1.0f, (float)cid,
        0.0f, 1.0f, 0.0f, (float)cid,
        0.0f, 0.0f, 0.0f, (float)cid
    };
}