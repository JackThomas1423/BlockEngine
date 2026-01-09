#include "occlusionCuller.hpp"
#include <iostream>

// ============================================================================
// OcclusionCuller Implementation
// ============================================================================

OcclusionCuller::OcclusionCuller() {
    // Pre-allocate query object pool
    queryPool.resize(1000);
    glGenQueries(queryPool.size(), queryPool.data());
}

OcclusionCuller::~OcclusionCuller() {
    if (!queryPool.empty()) {
        glDeleteQueries(queryPool.size(), queryPool.data());
    }
}

void OcclusionCuller::beginFrame() {
    visibleChunks = 0;
    culledChunks = 0;
    queriesThisFrame = 0;
    nextQueryIndex = 0;
    
    // Check results from previous queries
    checkQueryResults();
    
    // Clean up state for chunks that no longer exist
    cleanupOldQueries();
}

bool OcclusionCuller::testChunkVisibility(Chunk* chunk, const glm::vec3& chunkWorldPos) {
    if (!enabled) return true;
    
    auto it = chunkStates.find(chunk);
    
    // First time seeing this chunk - assume visible
    if (it == chunkStates.end()) {
        ChunkOcclusionState newState;
        newState.wasVisible = true;
        newState.framesInvisible = 0;
        newState.framesVisible = 0;
        newState.framesSinceQuery = 0;
        newState.queryActive = false;
        chunkStates[chunk] = newState;
        visibleChunks++;
        return true;
    }
    
    ChunkOcclusionState& state = it->second;
    state.framesSinceQuery++;
    
    // If we have an active query, check if result is ready (don't stall)
    if (state.queryActive) {
        GLuint available = 0;
        glGetQueryObjectuiv(state.queryID, GL_QUERY_RESULT_AVAILABLE, &available);
        
        if (available) {
            GLuint samplesPassed = 0;
            glGetQueryObjectuiv(state.queryID, GL_QUERY_RESULT, &samplesPassed);
            
            // Apply hysteresis in both directions
            if (samplesPassed > 0) {
                state.framesVisible++;
                state.framesInvisible = 0;
                
                // Become visible after being visible for 2+ frames
                if (!state.wasVisible && state.framesVisible >= 2) {
                    state.wasVisible = true;
                } else if (state.wasVisible) {
                    // Stay visible
                    state.wasVisible = true;
                }
            } else {
                state.framesInvisible++;
                state.framesVisible = 0;
                
                // Become invisible only after being invisible for 5+ frames
                if (state.wasVisible && state.framesInvisible >= 5) {
                    state.wasVisible = false;
                } else if (!state.wasVisible) {
                    // Stay invisible
                    state.wasVisible = false;
                }
            }
            
            state.queryActive = false;
            state.framesSinceQuery = 0;
        }
    }
    
    // Update stats
    if (state.wasVisible) {
        visibleChunks++;
    } else {
        culledChunks++;
    }
    
    return state.wasVisible;
}

void OcclusionCuller::submitOcclusionQuery(Chunk* chunk, const glm::vec3& chunkWorldPos, 
                                            const glm::mat4& projectionView, BoundingBoxRenderer* renderer) {
    if (!enabled) return;
    
    auto it = chunkStates.find(chunk);
    if (it == chunkStates.end()) return;
    
    ChunkOcclusionState& state = it->second;
    
    // Only query every N frames to reduce overhead and flickering
    if (state.framesSinceQuery < queryFrequency) {
        return;
    }
    
    // Don't start new query if one is already active
    if (state.queryActive) return;
    
    // Skip queries for chunks that just became visible (let them stay visible for a bit)
    if (state.wasVisible && state.framesInvisible == 0 && state.framesSinceQuery < queryFrequency * 2) {
        return;
    }
    
    // Get query object from pool
    state.queryID = getQueryObject();
    state.queryActive = true;
    queriesThisFrame++;
    
    // Start occlusion query
    glBeginQuery(GL_ANY_SAMPLES_PASSED, state.queryID);
    
    // Render bounding box
    glm::vec3 min = chunkWorldPos;
    glm::vec3 max = chunkWorldPos + glm::vec3(16.0f);
    if (renderer) {
        renderer->render(min, max, projectionView);
    }
    
    glEndQuery(GL_ANY_SAMPLES_PASSED);
}

void OcclusionCuller::endFrame() {
    // Nothing specific needed here, but available for future use
}

GLuint OcclusionCuller::getQueryObject() {
    if (nextQueryIndex >= queryPool.size()) {
        // Expand pool if needed
        size_t oldSize = queryPool.size();
        queryPool.resize(oldSize * 2);
        glGenQueries(queryPool.size() - oldSize, &queryPool[oldSize]);
    }
    return queryPool[nextQueryIndex++];
}

void OcclusionCuller::checkQueryResults() {
    // Results are checked in testChunkVisibility()
    // This function could batch-check all pending queries if needed
}

void OcclusionCuller::cleanupOldQueries() {
    // Remove state for chunks that haven't been tested in many frames
    std::vector<Chunk*> toRemove;
    for (auto& pair : chunkStates) {
        if (pair.second.framesSinceQuery > 300) {  // ~5 seconds at 60fps
            toRemove.push_back(pair.first);
        }
    }
    
    for (Chunk* chunk : toRemove) {
        chunkStates.erase(chunk);
    }
}

// ============================================================================
// BoundingBoxRenderer Implementation
// ============================================================================

BoundingBoxRenderer::BoundingBoxRenderer() {
    setupMesh();
    setupShader();
}

BoundingBoxRenderer::~BoundingBoxRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void BoundingBoxRenderer::setupMesh() {
    // Unit cube vertices (will be scaled and translated)
    float vertices[] = {
        0.0f, 0.0f, 0.0f,  // 0
        1.0f, 0.0f, 0.0f,  // 1
        1.0f, 1.0f, 0.0f,  // 2
        0.0f, 1.0f, 0.0f,  // 3
        0.0f, 0.0f, 1.0f,  // 4
        1.0f, 0.0f, 1.0f,  // 5
        1.0f, 1.0f, 1.0f,  // 6
        0.0f, 1.0f, 1.0f   // 7
    };
    
    // Indices for 12 triangles (2 per face)
    unsigned int indices[] = {
        0, 1, 2,  2, 3, 0,  // Front
        1, 5, 6,  6, 2, 1,  // Right
        5, 4, 7,  7, 6, 5,  // Back
        4, 0, 3,  3, 7, 4,  // Left
        3, 2, 6,  6, 7, 3,  // Top
        4, 5, 1,  1, 0, 4   // Bottom
    };
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void BoundingBoxRenderer::setupShader() {
    const char* vertexShaderSource = R"(
        #version 410 core
        layout (location = 0) in vec3 aPos;
        
        uniform mat4 projectionView;
        uniform vec3 minBounds;
        uniform vec3 maxBounds;
        
        void main() {
            vec3 worldPos = minBounds + aPos * (maxBounds - minBounds);
            gl_Position = projectionView * vec4(worldPos, 1.0);
        }
    )";
    
    const char* fragmentShaderSource = R"(
        #version 410 core
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";
    
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // Check for errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void BoundingBoxRenderer::render(const glm::vec3& min, const glm::vec3& max, 
                                  const glm::mat4& projectionView) {
    glUseProgram(shaderProgram);
    
    GLint pvLoc = glGetUniformLocation(shaderProgram, "projectionView");
    GLint minLoc = glGetUniformLocation(shaderProgram, "minBounds");
    GLint maxLoc = glGetUniformLocation(shaderProgram, "maxBounds");
    
    glUniformMatrix4fv(pvLoc, 1, GL_FALSE, &projectionView[0][0]);
    glUniform3fv(minLoc, 1, &min[0]);
    glUniform3fv(maxLoc, 1, &max[0]);
    
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}