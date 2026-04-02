#pragma once

#include <glm/glm.hpp>
#include <array>

// Raw (unnormalized) plane — only the sign of the normal matters for p-vertex
// selection, and the dot-product test is sign-equivalent without normalization.
// Skipping 6 sqrts per frame and keeping values in their natural scale.
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

    // Precomputed per-plane: the offset added to minPoint to get the p-vertex
    // for an AABB with a fixed size of 16. Computed once in update().
    // pVertexOffset[i] = dot(max(normal, 0), vec3(16)) per component.
    // For a cube chunk of side S: p-vertex dot = dot(n, min) + dot(max(n,0), vec3(S))
    float pVertexDot[6]; // = nx*(nx>0)*S + ny*(ny>0)*S + nz*(nz>0)*S, the fixed addend

    // For general AABB tests we still store sign masks (0 or 1 per component)
    // so callers can reconstruct the p-vertex cheaply.
    glm::vec3 pVertexMask[6]; // each component is 0.0 or 1.0

    void update(const glm::mat4& m) {
        // Gribb/Hartmann extraction (column-major GLM layout)
        // No normalization — sign of distance is all we need.
        planes[LEFT].normal.x  =  m[0][3] + m[0][0];
        planes[LEFT].normal.y  =  m[1][3] + m[1][0];
        planes[LEFT].normal.z  =  m[2][3] + m[2][0];
        planes[LEFT].distance  =  m[3][3] + m[3][0];

        planes[RIGHT].normal.x =  m[0][3] - m[0][0];
        planes[RIGHT].normal.y =  m[1][3] - m[1][0];
        planes[RIGHT].normal.z =  m[2][3] - m[2][0];
        planes[RIGHT].distance =  m[3][3] - m[3][0];

        planes[BOTTOM].normal.x = m[0][3] + m[0][1];
        planes[BOTTOM].normal.y = m[1][3] + m[1][1];
        planes[BOTTOM].normal.z = m[2][3] + m[2][1];
        planes[BOTTOM].distance = m[3][3] + m[3][1];

        planes[TOP].normal.x   =  m[0][3] - m[0][1];
        planes[TOP].normal.y   =  m[1][3] - m[1][1];
        planes[TOP].normal.z   =  m[2][3] - m[2][1];
        planes[TOP].distance   =  m[3][3] - m[3][1];

        planes[NEAR].normal.x  =  m[0][3] + m[0][2];
        planes[NEAR].normal.y  =  m[1][3] + m[1][2];
        planes[NEAR].normal.z  =  m[2][3] + m[2][2];
        planes[NEAR].distance  =  m[3][3] + m[3][2];

        planes[FAR].normal.x   =  m[0][3] - m[0][2];
        planes[FAR].normal.y   =  m[1][3] - m[1][2];
        planes[FAR].normal.z   =  m[2][3] - m[2][2];
        planes[FAR].distance   =  m[3][3] - m[3][2];

        // Precompute the fixed chunk-size addend for isChunkVisible.
        // For each plane: pVertexDot = sum of (normal[i] > 0 ? 16 : 0)
        // This is the part of dot(p-vertex, normal) that doesn't depend on
        // the chunk's world position, so we only pay for it once per frame.
        constexpr float S = 16.0f;
        for (int i = 0; i < 6; ++i) {
            const glm::vec3& n = planes[i].normal;
            pVertexMask[i] = glm::vec3(
                n.x >= 0.0f ? 1.0f : 0.0f,
                n.y >= 0.0f ? 1.0f : 0.0f,
                n.z >= 0.0f ? 1.0f : 0.0f
            );
            pVertexDot[i] = (n.x >= 0.0f ? n.x : 0.0f) * S
                          + (n.y >= 0.0f ? n.y : 0.0f) * S
                          + (n.z >= 0.0f ? n.z : 0.0f) * S;
        }
    }

    // General AABB test. Uses precomputed sign masks to pick the p-vertex.
    inline bool isBoxVisible(const glm::vec3& minPoint, const glm::vec3& maxPoint) const {
        for (int i = 0; i < 6; ++i) {
            const Plane& p = planes[i];
            const glm::vec3& mask = pVertexMask[i];

            // p-vertex: per component, pick max if normal >= 0, else min
            float px = minPoint.x + mask.x * (maxPoint.x - minPoint.x);
            float py = minPoint.y + mask.y * (maxPoint.y - minPoint.y);
            float pz = minPoint.z + mask.z * (maxPoint.z - minPoint.z);

            if (p.normal.x * px + p.normal.y * py + p.normal.z * pz + p.distance < 0.0f)
                return false;
        }
        return true;
    }

    // Fast path for axis-aligned cubic chunks of fixed size 16.
    //
    // The p-vertex dot product splits into two parts:
    //   dot(n, p-vertex) = dot(n, minPoint) + pVertexDot[i]
    //
    // pVertexDot[i] was precomputed in update() and doesn't change per chunk,
    // so each plane test costs: 3 muls + 2 adds + 1 compare.
    // Plane order: NEAR, FAR, LEFT, RIGHT, TOP, BOTTOM — most likely to cull first.
    inline bool isChunkVisible(const glm::vec3& minPoint, float /*chunkSize*/ = 16.0f) const {
        // NEAR
        {
            const Plane& p = planes[NEAR];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[NEAR] + p.distance < 0.0f) return false;
        }
        // FAR
        {
            const Plane& p = planes[FAR];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[FAR] + p.distance < 0.0f) return false;
        }
        // LEFT
        {
            const Plane& p = planes[LEFT];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[LEFT] + p.distance < 0.0f) return false;
        }
        // RIGHT
        {
            const Plane& p = planes[RIGHT];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[RIGHT] + p.distance < 0.0f) return false;
        }
        // TOP
        {
            const Plane& p = planes[TOP];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[TOP] + p.distance < 0.0f) return false;
        }
        // BOTTOM
        {
            const Plane& p = planes[BOTTOM];
            if (p.normal.x * minPoint.x + p.normal.y * minPoint.y + p.normal.z * minPoint.z
                + pVertexDot[BOTTOM] + p.distance < 0.0f) return false;
        }
        return true;
    }
};