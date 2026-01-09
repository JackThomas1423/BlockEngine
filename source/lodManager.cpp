#include "lodManager.hpp"
#include <glm/gtc/constants.hpp>

LODManager::LODManager() {
    // Use default config from struct
}

LODLevel LODManager::calculateLOD(const glm::vec3& chunkWorldPos, 
                                   const glm::vec3& cameraPos, 
                                   LODLevel currentLOD) const {
    glm::vec3 chunkCenter = chunkWorldPos + glm::vec3(8.0f);
    float distance = glm::length(chunkCenter - cameraPos);
    
    // Apply hysteresis to prevent LOD thrashing
    float threshold0 = config.distances[0] + (currentLOD > LOD_0 ? -config.hysteresis : config.hysteresis);
    float threshold1 = config.distances[1] + (currentLOD > LOD_1 ? -config.hysteresis : config.hysteresis);
    float threshold2 = config.distances[2] + (currentLOD > LOD_2 ? -config.hysteresis : config.hysteresis);
    
    if (distance < threshold0) return LOD_0;
    if (distance < threshold1) return LOD_1;
    if (distance < threshold2) return LOD_2;
    return LOD_3;
}

void LODManager::setLODDistance(int lodLevel, float distance) {
    if (lodLevel >= 0 && lodLevel < 4) {
        config.distances[lodLevel] = distance;
    }
}

void LODManager::setHysteresis(float hysteresis) {
    config.hysteresis = hysteresis;
}