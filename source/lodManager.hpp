#pragma once

#include <glm/glm.hpp>
#include "chunk.hpp"

struct LODConfig {
    float distances[4] = {
        512.0f,   // LOD_0 threshold
        1024.0f,   // LOD_1 threshold
        2048.0f,  // LOD_2 threshold
        4096.0f   // LOD_3 threshold
    };
    float hysteresis = 32.0f; // Prevents rapid LOD switching
};

class LODManager {
public:
    LODManager();
    
    LODLevel calculateLOD(const glm::vec3& chunkWorldPos, 
                         const glm::vec3& cameraPos, 
                         LODLevel currentLOD) const;
    
    void setLODDistance(int lodLevel, float distance);
    void setHysteresis(float hysteresis);
    
    const LODConfig& getConfig() const { return config; }
    
private:
    LODConfig config;
};