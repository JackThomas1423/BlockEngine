#pragma once

#include <glm/glm.hpp>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <set>
#include "chunk.hpp"
#include "threadPool.hpp"

struct ivec3Compare {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

struct ChunkLoadTask {
    glm::ivec3 position;
    
    // Helper function to calculate distance dynamically
    float getDistance(const glm::vec3& cameraPos) const {
        glm::vec3 chunkWorldPos = glm::vec3(position) * 16.0f + glm::vec3(8.0f);
        return glm::length(chunkWorldPos - cameraPos);
    }
    
    bool operator<(const ChunkLoadTask& other) const {
        // This comparison is now only used for initial sorting
        // Distance will be recalculated at execution time
        return false;
    }
};

class WorldManager {
public:
    WorldManager(int renderDistance = 32);
    ~WorldManager();
    
    void start(ThreadPool* threadPool);
    void stop();
    void updateCameraPosition(const glm::vec3& cameraPosition);
    
    ChunkMap& getChunkMap() { return chunk_map; }
    std::shared_mutex& getChunkMapMutex() { return chunk_map_mutex; }
    
    int getLoadedChunkCount() const;
    int getLoadingChunkCount() const;
    
    glm::vec3 getCurrentCameraPosition() const;
    
private:
    void unloadDistantChunks(const glm::ivec3& cameraChunk);
    void queueChunksForLoading(const glm::ivec3& cameraChunk, const glm::vec3& cameraPos);
    void pruneOutOfRangeLoadingChunks(const glm::ivec3& cameraChunk);
    
    ChunkMap chunk_map;
    std::shared_mutex chunk_map_mutex;
    
    std::set<glm::ivec3, ivec3Compare> chunksLoading;
    std::set<glm::ivec3, ivec3Compare> chunksLoaded;
    std::set<glm::ivec3, ivec3Compare> chunksProcessed; // Tracks all processed chunks (loaded, empty, or skipped)
    std::mutex loadingMutex;
    
    std::atomic<bool> running{false};
    std::atomic<int> pendingTaskCount{0};
    
    glm::vec3 currentCameraPosition{0.0f};
    std::mutex cameraPosMutex;
    
    ThreadPool* pool = nullptr;
    
    const int RENDER_DISTANCE;
    const int MAX_PENDING_TASKS = 48 * 48 * 48;
    
    void gameLoop();
    std::thread gameThread;
};