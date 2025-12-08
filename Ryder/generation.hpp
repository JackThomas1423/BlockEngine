#pragma once

#include <glm/gtc/noise.hpp>
#include <glm/glm.hpp>
#include <vector>
#include "../Jack/voxel.hpp"

void genChunk(Chunk& chunk);
// Set global seed for noise generation. Call this once before generating chunks to
// get reproducible worlds.
void setNoiseSeed(uint32_t seed);