#pragma once

#include <glm/glm.hpp>
#include <atomic>
#include <mutex>
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
    float distance;
    
    bool operator<(const ChunkLoadTask& other) const {
        return distance < other.distance;
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
    std::mutex& getChunkMapMutex() { return chunk_map_mutex; }
    
    int getLoadedChunkCount() const;
    int getLoadingChunkCount() const;
    
private:
    void unloadDistantChunks(const glm::ivec3& cameraChunk);
    void queueChunksForLoading(const glm::ivec3& cameraChunk, const glm::vec3& cameraPos);
    
    ChunkMap chunk_map;
    std::mutex chunk_map_mutex;
    
    std::set<glm::ivec3, ivec3Compare> chunksLoading;
    std::set<glm::ivec3, ivec3Compare> chunksLoaded;
    std::mutex loadingMutex;
    
    std::atomic<bool> running{false};
    std::atomic<int> pendingTaskCount{0};
    
    glm::vec3 currentCameraPosition{0.0f};
    std::mutex cameraPosMutex;
    
    ThreadPool* pool = nullptr;
    
    const int RENDER_DISTANCE;
    const int MAX_PENDING_TASKS = 32 * 32 * 32;
    
    void gameLoop();
    std::thread gameThread;
};