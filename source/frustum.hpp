#pragma once

#include <glm/glm.hpp>
#include <array>

struct Plane {
    glm::vec3 normal;
    float distance;
    
    Plane() : normal(0.0f), distance(0.0f) {}
    
    inline float distanceToPoint(const glm::vec3& point) const {
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
    
    // Optimized AABB test with early-out
    inline bool isBoxVisible(const glm::vec3& minPoint, const glm::vec3& maxPoint) const {
        // Test against each plane
        // Most chunks fail on near/far planes first, so test those early
        for (int i : {NEAR, FAR, LEFT, RIGHT, TOP, BOTTOM}) {
            const Plane& plane = planes[i];
            
            // Get positive vertex (furthest in direction of plane normal)
            // Using direct comparisons instead of conditionals for branch prediction
            glm::vec3 positiveVertex;
            positiveVertex.x = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            positiveVertex.y = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            positiveVertex.z = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            
            // Early out if outside this plane
            if (plane.distanceToPoint(positiveVertex) < 0) {
                return false;
            }
        }
        return true;
    }
    
    // Optimized chunk visibility test
    inline bool isChunkVisible(const glm::vec3& chunkWorldPos, float chunkSize = 16.0f) const {
        // Pre-compute min/max once
        const glm::vec3 minPoint = chunkWorldPos;
        const glm::vec3 maxPoint = chunkWorldPos + chunkSize;
        
        // Unrolled loop for better performance - test most discriminating planes first
        // Near plane (most chunks fail here)
        {
            const Plane& plane = planes[NEAR];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        // Far plane
        {
            const Plane& plane = planes[FAR];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        // Left plane
        {
            const Plane& plane = planes[LEFT];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        // Right plane
        {
            const Plane& plane = planes[RIGHT];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        // Top plane
        {
            const Plane& plane = planes[TOP];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        // Bottom plane
        {
            const Plane& plane = planes[BOTTOM];
            float px = (plane.normal.x >= 0) ? maxPoint.x : minPoint.x;
            float py = (plane.normal.y >= 0) ? maxPoint.y : minPoint.y;
            float pz = (plane.normal.z >= 0) ? maxPoint.z : minPoint.z;
            if (plane.normal.x * px + plane.normal.y * py + plane.normal.z * pz + plane.distance < 0) {
                return false;
            }
        }
        
        return true;
    }
};