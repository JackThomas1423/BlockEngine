#pragma once

#include <glm/glm.hpp>
#include <array>

struct Plane {
    glm::vec3 normal;
    float distance;
    
    Plane() : normal(0.0f), distance(0.0f) {}
    
    float distanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

class Frustum {
public:
    enum {
        LEFT = 0,
        RIGHT,
        BOTTOM,
        TOP,
        NEAR,
        FAR
    };
    
    std::array<Plane, 6> planes;
    
    void update(const glm::mat4& projectionViewMatrix) {
        // Extract frustum planes from projection-view matrix
        // Left plane
        planes[LEFT].normal.x = projectionViewMatrix[0][3] + projectionViewMatrix[0][0];
        planes[LEFT].normal.y = projectionViewMatrix[1][3] + projectionViewMatrix[1][0];
        planes[LEFT].normal.z = projectionViewMatrix[2][3] + projectionViewMatrix[2][0];
        planes[LEFT].distance = projectionViewMatrix[3][3] + projectionViewMatrix[3][0];
        
        // Right plane
        planes[RIGHT].normal.x = projectionViewMatrix[0][3] - projectionViewMatrix[0][0];
        planes[RIGHT].normal.y = projectionViewMatrix[1][3] - projectionViewMatrix[1][0];
        planes[RIGHT].normal.z = projectionViewMatrix[2][3] - projectionViewMatrix[2][0];
        planes[RIGHT].distance = projectionViewMatrix[3][3] - projectionViewMatrix[3][0];
        
        // Bottom plane
        planes[BOTTOM].normal.x = projectionViewMatrix[0][3] + projectionViewMatrix[0][1];
        planes[BOTTOM].normal.y = projectionViewMatrix[1][3] + projectionViewMatrix[1][1];
        planes[BOTTOM].normal.z = projectionViewMatrix[2][3] + projectionViewMatrix[2][1];
        planes[BOTTOM].distance = projectionViewMatrix[3][3] + projectionViewMatrix[3][1];
        
        // Top plane
        planes[TOP].normal.x = projectionViewMatrix[0][3] - projectionViewMatrix[0][1];
        planes[TOP].normal.y = projectionViewMatrix[1][3] - projectionViewMatrix[1][1];
        planes[TOP].normal.z = projectionViewMatrix[2][3] - projectionViewMatrix[2][1];
        planes[TOP].distance = projectionViewMatrix[3][3] - projectionViewMatrix[3][1];
        
        // Near plane
        planes[NEAR].normal.x = projectionViewMatrix[0][3] + projectionViewMatrix[0][2];
        planes[NEAR].normal.y = projectionViewMatrix[1][3] + projectionViewMatrix[1][2];
        planes[NEAR].normal.z = projectionViewMatrix[2][3] + projectionViewMatrix[2][2];
        planes[NEAR].distance = projectionViewMatrix[3][3] + projectionViewMatrix[3][2];
        
        // Far plane
        planes[FAR].normal.x = projectionViewMatrix[0][3] - projectionViewMatrix[0][2];
        planes[FAR].normal.y = projectionViewMatrix[1][3] - projectionViewMatrix[1][2];
        planes[FAR].normal.z = projectionViewMatrix[2][3] - projectionViewMatrix[2][2];
        planes[FAR].distance = projectionViewMatrix[3][3] - projectionViewMatrix[3][2];
        
        // Normalize all planes
        for (int i = 0; i < 6; i++) {
            float length = glm::length(planes[i].normal);
            planes[i].normal /= length;
            planes[i].distance /= length;
        }
    }
    
    // Test if an AABB (axis-aligned bounding box) is inside or intersecting the frustum
    bool isBoxVisible(const glm::vec3& minPoint, const glm::vec3& maxPoint) const {
        for (int i = 0; i < 6; i++) {
            // Get positive vertex (the one furthest in the direction of the plane normal)
            glm::vec3 positiveVertex = minPoint;
            if (planes[i].normal.x >= 0) positiveVertex.x = maxPoint.x;
            if (planes[i].normal.y >= 0) positiveVertex.y = maxPoint.y;
            if (planes[i].normal.z >= 0) positiveVertex.z = maxPoint.z;
            
            // If the positive vertex is outside this plane, the box is outside the frustum
            if (planes[i].distanceToPoint(positiveVertex) < 0) {
                return false;
            }
        }
        return true;
    }
    
    // Overload for chunk position (assuming 16x16x16 chunk size)
    bool isChunkVisible(const glm::vec3& chunkWorldPos, float chunkSize = 16.0f) const {
        glm::vec3 minPoint = chunkWorldPos;
        glm::vec3 maxPoint = chunkWorldPos + glm::vec3(chunkSize);
        return isBoxVisible(minPoint, maxPoint);
    }
};