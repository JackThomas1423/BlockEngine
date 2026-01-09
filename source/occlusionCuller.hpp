#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>
#include <unordered_map>
#include "chunk.hpp"

struct ChunkOcclusionState {
    GLuint queryID = 0;
    bool queryActive = false;
    bool wasVisible = true;  // Assume visible until proven otherwise
    int framesSinceQuery = 0;
    int framesInvisible = 0;  // Counter to avoid flickering when becoming invisible
    int framesVisible = 0;    // Counter to avoid flickering when becoming visible
};

class BoundingBoxRenderer;

class OcclusionCuller {
public:
    OcclusionCuller();
    ~OcclusionCuller();
    
    // Begin occlusion testing for this frame
    void beginFrame();
    
    // Test if a chunk is visible
    // Returns true if chunk should be rendered this frame
    bool testChunkVisibility(Chunk* chunk, const glm::vec3& chunkWorldPos);
    
    // Submit a bounding box for occlusion testing
    void submitOcclusionQuery(Chunk* chunk, const glm::vec3& chunkWorldPos,
                             const glm::mat4& projectionView, BoundingBoxRenderer* renderer);
    
    // End occlusion testing and process results
    void endFrame();
    
    // Configuration
    void setQueryFrequency(int frames) { queryFrequency = frames; }
    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }
    
    // Stats
    int getVisibleChunks() const { return visibleChunks; }
    int getCulledChunks() const { return culledChunks; }
    int getQueriesThisFrame() const { return queriesThisFrame; }
    
private:
    // Pool of query objects for reuse
    std::vector<GLuint> queryPool;
    int nextQueryIndex = 0;
    
    // Track occlusion state per chunk
    std::unordered_map<Chunk*, ChunkOcclusionState> chunkStates;
    
    // Configuration
    bool enabled = true;
    int queryFrequency = 10;  // Test every N frames (reduced from 1 to avoid flickering)
    
    // Stats
    int visibleChunks = 0;
    int culledChunks = 0;
    int queriesThisFrame = 0;
    
    // Helper methods
    GLuint getQueryObject();
    void checkQueryResults();
    void cleanupOldQueries();
};

// Simple bounding box renderer for occlusion queries
class BoundingBoxRenderer {
public:
    BoundingBoxRenderer();
    ~BoundingBoxRenderer();
    
    void render(const glm::vec3& min, const glm::vec3& max, 
                const glm::mat4& projectionView);
    
private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLuint shaderProgram = 0;
    
    void setupMesh();
    void setupShader();
};