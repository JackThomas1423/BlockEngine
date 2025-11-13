#include "voxel.hpp"

const glm::ivec3 faceNormals[] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1}
};

const glm::vec3 faceVerts[6][4] = {
    // +X (looking from positive X toward origin)
    {{1,0,0},{1,1,0},{1,1,1},{1,0,1}},
    // -X (looking from negative X toward origin)
    {{0,0,1},{0,1,1},{0,1,0},{0,0,0}},
    // +Y (looking from positive Y toward origin)
    {{1,1,0},{0,1,0},{0,1,1},{1,1,1}},
    // -Y (looking from negative Y toward origin)
    {{1,0,1},{0,0,1},{0,0,0},{1,0,0}},
    // +Z (looking from positive Z toward origin)
    {{1,0,1},{1,1,1},{0,1,1},{0,0,1}},
    // -Z (looking from negative Z toward origin)
    {{0,0,0},{0,1,0},{1,1,0},{1,0,0}},
};


unsigned int Chunk::addVertex(const Vertex& vertex) {
    auto it = vertexMap.find(vertex);
    if (it != vertexMap.end()) return it->second;

    unsigned int index = vertices.size() / 4; // 4 is the number of floats per vertex
    vertices.push_back(vertex.position.x);
    vertices.push_back(vertex.position.y);
    vertices.push_back(vertex.position.z);
    vertices.push_back(static_cast<float>(vertex.color)); // store color as float for simplicity

    vertexMap[vertex] = index;
    return index;
}

void Chunk::addFace(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    unsigned int i0 = addVertex(v0);
    unsigned int i1 = addVertex(v1);
    unsigned int i2 = addVertex(v2);
    unsigned int i3 = addVertex(v3);

    // Two triangles per face - CCW winding
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);

    indices.push_back(i0);
    indices.push_back(i2);
    indices.push_back(i3);
}

Chunk::Chunk(glm::vec3 position) {
    global_position = position;
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                data[x][y][z] = 0; // initialize all voxels to air
            }
        }
    }
}

Mesh Chunk::computeMesh() {
    float startTime = static_cast<float>(glfwGetTime());
    int totalPotentialFaces = 0;
    int renderedFaces = 0;

    // Culling
    auto isSolid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= CHUNK_WIDTH)  return false;
        if (y < 0 || y >= CHUNK_HEIGHT) return false;
        if (z < 0 || z >= CHUNK_DEPTH)  return false;
        return data[x][y][z] != 0;
    };

    // Greedy meshing for each axis and direction
    for (int axis = 0; axis < 3; ++axis) {
        for (int direction = -1; direction <= 1; direction += 2) {
            // axis: 0=X, 1=Y, 2=Z
            // direction: -1 or +1
            
            int u = (axis + 1) % 3;  // first perpendicular axis
            int v = (axis + 2) % 3;  // second perpendicular axis
            
            int dims[3] = { CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH };
            
            // Mask to track which voxels have been meshed
            bool mask[CHUNK_WIDTH * CHUNK_HEIGHT];
            ColorId colorMask[CHUNK_WIDTH * CHUNK_HEIGHT];
            
            // Iterate through slices perpendicular to the axis
            for (int d = -1; d < dims[axis]; ++d) {
                // Build the mask for this slice
                std::fill(mask, mask + dims[u] * dims[v], false);
                std::fill(colorMask, colorMask + dims[u] * dims[v], 0);
                
                for (int j = 0; j < dims[v]; ++j) {
                    for (int i = 0; i < dims[u]; ++i) {
                        int pos[3], neighbor[3];
                        pos[axis] = d;
                        pos[u] = i;
                        pos[v] = j;
                        
                        neighbor[axis] = d + direction;
                        neighbor[u] = i;
                        neighbor[v] = j;
                        
                        bool currentSolid = isSolid(pos[0], pos[1], pos[2]);
                        bool neighborSolid = isSolid(neighbor[0], neighbor[1], neighbor[2]);
                        ColorId currentColor = (pos[axis] >= 0 && pos[axis] < dims[axis]) 
                            ? data[pos[0]][pos[1]][pos[2]] : 0;
                        
                        totalPotentialFaces++;
                        
                        // Face is visible if current is solid and neighbor is not
                        if (currentSolid && !neighborSolid && currentColor != 0) {
                            mask[i + j * dims[u]] = true;
                            colorMask[i + j * dims[u]] = currentColor;
                        }
                    }
                }
                
                // Generate mesh from mask using greedy meshing
                for (int j = 0; j < dims[v]; ++j) {
                    for (int i = 0; i < dims[u]; ) {
                        if (!mask[i + j * dims[u]]) {
                            ++i;
                            continue;
                        }
                        
                        ColorId currentColor = colorMask[i + j * dims[u]];
                        
                        // Compute width
                        int w;
                        for (w = 1; i + w < dims[u]; ++w) {
                            if (!mask[i + w + j * dims[u]] || 
                                colorMask[i + w + j * dims[u]] != currentColor) {
                                break;
                            }
                        }
                        
                        // Compute height
                        int h;
                        bool done = false;
                        for (h = 1; j + h < dims[v]; ++h) {
                            for (int k = 0; k < w; ++k) {
                                if (!mask[i + k + (j + h) * dims[u]] ||
                                    colorMask[i + k + (j + h) * dims[u]] != currentColor) {
                                    done = true;
                                    break;
                                }
                            }
                            if (done) break;
                        }
                        
                        // Create quad
                        glm::vec3 v0, v1, v2, v3;
                        int p[3];
                        p[axis] = d + (direction > 0 ? 1 : 0);
                        p[u] = i;
                        p[v] = j;
                        
                        glm::vec3 du(0), dv(0);
                        du[u] = w;
                        dv[v] = h;
                        
                        glm::vec3 base = global_position + glm::vec3(p[0], p[1], p[2]);
                        
                        if (direction > 0) {
                            v0 = base;
                            v1 = base + du;
                            v2 = base + du + dv;
                            v3 = base + dv;
                        } else {
                            v0 = base + dv;
                            v1 = base + du + dv;
                            v2 = base + du;
                            v3 = base;
                        }
                        
                        addFace(
                            { v0, currentColor },
                            { v1, currentColor },
                            { v2, currentColor },
                            { v3, currentColor }
                        );
                        
                        renderedFaces++;
                        
                        // Clear the mask for the merged quad
                        for (int l = 0; l < h; ++l) {
                            for (int k = 0; k < w; ++k) {
                                mask[i + k + (j + l) * dims[u]] = false;
                            }
                        }
                        
                        i += w;
                    }
                }
            }
        }
    }

    float endTime = static_cast<float>(glfwGetTime());
    float meshTime = (endTime - startTime) * 1000.0f;
    std::cout << "Chunk meshing time: " << meshTime << " ms" << std::endl;
    int culledFaces = totalPotentialFaces - renderedFaces;
    float percentCulled = (totalPotentialFaces > 0) ? 
        (culledFaces * 100.0f / totalPotentialFaces) : 0.0f;
    
    std::cout << "Chunk meshing: " << renderedFaces << "/" << totalPotentialFaces 
              << " faces rendered (" << percentCulled << "% culled)" << std::endl;

    return Mesh { vertices, indices };
}