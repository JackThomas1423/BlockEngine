#pragma once

#include "chunk.hpp"
#include "threadPool.hpp"
#include <atomic>
#include <glm/glm.hpp>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <vector>

struct ivec3Compare {
  bool operator()(const glm::ivec3 &a, const glm::ivec3 &b) const {
    if (a.x != b.x)
      return a.x < b.x;
    if (a.y != b.y)
      return a.y < b.y;
    return a.z < b.z;
  }
};

struct ChunkLoadTask {
  glm::ivec3 position;

  float getDistance(const glm::vec3 &cameraPos) const {
    glm::vec3 chunkWorldPos = glm::vec3(position) * 16.0f + glm::vec3(8.0f);
    return glm::length(chunkWorldPos - cameraPos);
  }

  bool operator<(const ChunkLoadTask &other) const { return false; }
};

class WorldManager {
public:
  WorldManager(int renderDistance = 32);
  ~WorldManager();

  void start(ThreadPool *loadingThreadPool, ThreadPool *updateThreadPool);
  void stop();
  void updateCameraPosition(const glm::vec3 &cameraPosition);
  void cleanUpDeletedChunks();

  ChunkMap &getChunkMap() { return chunk_map; }
  std::shared_mutex &getChunkMapMutex() { return chunk_map_mutex; }

  // --- Culling interface ---
  // activeChunks and activeChunkWorldPos are parallel arrays kept in sync.
  // The culling loop reads only activeChunkWorldPos (hot floats, no pointer
  // chasing) and looks up activeChunks[i] only for chunks that pass the frustum.
  const std::vector<Chunk *>    &getActiveChunks()        const { return activeChunks; }
  const std::vector<glm::vec3>  &getActiveChunkWorldPos() const { return activeChunkWorldPos; }
  std::shared_mutex             &getActiveChunksMutex()   const { return activeChunksMutex; }

  void queueMeshUpdate(Chunk *chunk);

  int getLoadedChunkCount() const;
  int getLoadingChunkCount() const;
  int getRenderDistance() const { return RENDER_DISTANCE; }

  glm::vec3 getCurrentCameraPosition() const;

private:
  void unloadDistantChunks(const glm::ivec3 &cameraChunk);
  void queueChunksForLoading(const glm::ivec3 &cameraChunk,
                             const glm::vec3 &cameraPos);
  void pruneOutOfRangeLoadingChunks(const glm::ivec3 &cameraChunk);

  void onChunkLoaded(Chunk *chunk);
  void unloadChunk(const glm::ivec3 &pos);

  ChunkMap chunk_map;
  std::shared_mutex chunk_map_mutex;

  std::vector<glm::ivec3> loadingOffsets;
  void initLoadingOffsets();

  // Parallel arrays — always kept in sync under activeChunksMutex.
  // activeChunkWorldPos[i] == glm::vec3(activeChunks[i]->chunkPosition) * 16.f
  // Keeping world positions in a flat float array gives the culling loop
  // sequential cache-friendly access without touching the large Chunk objects.
  std::vector<Chunk *>   activeChunks;
  std::vector<glm::vec3> activeChunkWorldPos;
  mutable std::shared_mutex activeChunksMutex;

  std::vector<Chunk *> chunksToDelete;
  std::mutex delete_queue_mutex;

  std::set<glm::ivec3, ivec3Compare> chunksLoading;
  std::set<glm::ivec3, ivec3Compare> chunksLoaded;
  std::set<glm::ivec3, ivec3Compare> chunksProcessed;
  mutable std::mutex loadingMutex;

  std::atomic<bool> running{false};
  std::atomic<int> pendingTaskCount{0};

  glm::vec3 currentCameraPosition{0.0f};
  mutable std::mutex cameraPosMutex;

  ThreadPool *loadingPool = nullptr;
  ThreadPool *updatePool = nullptr;

  const int RENDER_DISTANCE;
  const int MAX_PENDING_TASKS = 32 * 32 * 32;

  void gameLoop();
  std::thread gameThread;
};