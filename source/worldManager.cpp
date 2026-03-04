#include "worldManager.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>


WorldManager::WorldManager(int renderDistance)
    : RENDER_DISTANCE(renderDistance) {}

WorldManager::~WorldManager() {
  stop();
  // Clean up chunks with exclusive lock
  std::unique_lock<std::shared_mutex> lock(chunk_map_mutex);
  for (auto &pair : chunk_map) {
    delete pair.second;
  }
  chunk_map.clear();
}

void WorldManager::start(ThreadPool *loadingThreadPool,
                         ThreadPool *updateThreadPool) {
  loadingPool = loadingThreadPool;
  updatePool = updateThreadPool;
  running = true;
  gameThread = std::thread(&WorldManager::gameLoop, this);
}

void WorldManager::stop() {
  running = false;
  if (gameThread.joinable()) {
    gameThread.join();
  }
}

void WorldManager::updateCameraPosition(const glm::vec3 &cameraPosition) {
  std::lock_guard<std::mutex> lock(cameraPosMutex);
  currentCameraPosition = cameraPosition;
}

glm::vec3 WorldManager::getCurrentCameraPosition() const {
  std::lock_guard<std::mutex> lock(cameraPosMutex);
  return currentCameraPosition;
}

void WorldManager::gameLoop() {
  const float UPDATE_INTERVAL = 0.01f;
  auto lastUpdateTime = std::chrono::high_resolution_clock::now();

  while (running) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime =
        std::chrono::duration<float>(currentTime - lastUpdateTime).count();

    if (deltaTime >= UPDATE_INTERVAL) {
      lastUpdateTime = currentTime;

      glm::vec3 cameraPos = getCurrentCameraPosition();

      glm::ivec3 cameraChunk =
          glm::ivec3(floor(cameraPos.x / 16.0f), floor(cameraPos.y / 16.0f),
                     floor(cameraPos.z / 16.0f));

      unloadDistantChunks(cameraChunk);
      pruneOutOfRangeLoadingChunks(cameraChunk);
      queueChunksForLoading(cameraChunk, cameraPos);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void WorldManager::unloadDistantChunks(const glm::ivec3 &cameraChunk) {
  std::vector<glm::ivec3> chunksToUnload;

  {
    std::shared_lock<std::shared_mutex> lock(chunk_map_mutex);

    for (const auto &[pos, chunk] : chunk_map) {
      int dx = std::abs(pos.x - cameraChunk.x);
      int dy = std::abs(pos.y - cameraChunk.y);
      int dz = std::abs(pos.z - cameraChunk.z);
      int maxDist = std::max({dx, dy, dz});

      if (maxDist > RENDER_DISTANCE + 4) {
        chunksToUnload.push_back(pos);
      }
    }
  }

  // Unload chunks using the safe two-phase protocol
  for (const auto &pos : chunksToUnload) {
    unloadChunk(pos); // Uses the safe deletion we defined above
  }
}

void WorldManager::pruneOutOfRangeLoadingChunks(const glm::ivec3 &cameraChunk) {
  std::vector<glm::ivec3> chunksToPrune;

  {
    std::lock_guard<std::mutex> lock(loadingMutex);
    for (const auto &chunkPos : chunksLoading) {
      glm::ivec3 diff = chunkPos - cameraChunk;
      int horizontalDist = std::max(std::abs(diff.x), std::abs(diff.z));

      if (horizontalDist > RENDER_DISTANCE + 2 ||
          chunkPos.y < cameraChunk.y - 4 - 2 ||
          chunkPos.y > cameraChunk.y + 7 + 2) {
        chunksToPrune.push_back(chunkPos);
      }
    }

    for (const auto &pos : chunksToPrune) {
      chunksLoading.erase(pos);
      chunksProcessed.insert(pos);
    }
  }

  if (!chunksToPrune.empty()) {
    std::cout << "Pruned " << chunksToPrune.size()
              << " out-of-range chunks from loading queue" << std::endl;
  }
}

void WorldManager::queueChunksForLoading(const glm::ivec3 &cameraChunk,
                                         const glm::vec3 &cameraPos) {
  std::vector<ChunkLoadTask> tasksToLoad;
  int maxTerrainChunkY = (MAX_HEIGHT / CHUNK_HEIGHT) + 1;

  int totalQueued = 0;
  for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
    for (int y = -4; y <= 7; ++y) {
      for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
        glm::ivec3 chunkPos = cameraChunk + glm::ivec3(x, y, z);

        if (chunkPos.y > maxTerrainChunkY)
          continue;

        {
          std::lock_guard<std::mutex> lock(loadingMutex);
          if (chunksProcessed.find(chunkPos) != chunksProcessed.end()) {
            continue;
          }
          if (chunksLoading.find(chunkPos) != chunksLoading.end()) {
            continue;
          }

          chunksLoading.insert(chunkPos);
        }

        tasksToLoad.push_back({chunkPos});
        totalQueued++;
      }
    }
  }

  if (totalQueued == 0)
    return;

  std::sort(tasksToLoad.begin(), tasksToLoad.end(),
            [&cameraPos](const ChunkLoadTask &a, const ChunkLoadTask &b) {
              return a.getDistance(cameraPos) < b.getDistance(cameraPos);
            });

  size_t tasksSubmitted = 0;
  for (const auto &task : tasksToLoad) {
    if (pendingTaskCount >= MAX_PENDING_TASKS) {
      std::lock_guard<std::mutex> lock(loadingMutex);
      chunksLoading.erase(task.position);
      continue;
    }

    pendingTaskCount++;
    tasksSubmitted++;

    loadingPool->enqueue([task, this]() {
      // RAII guard for task count
      struct Guard {
        std::atomic<int> &c;
        Guard(std::atomic<int> &cnt) : c(cnt) {}
        ~Guard() { c--; }
      } guard(pendingTaskCount);

      glm::vec3 currentCamPos = getCurrentCameraPosition();

      glm::ivec3 currentCameraChunk = glm::ivec3(
          floor(currentCamPos.x / 16.0f), floor(currentCamPos.y / 16.0f),
          floor(currentCamPos.z / 16.0f));
      glm::ivec3 diff = task.position - currentCameraChunk;
      int horizontalDist = std::max(std::abs(diff.x), std::abs(diff.z));

      if (horizontalDist > RENDER_DISTANCE + 2 ||
          task.position.y < currentCameraChunk.y - 4 - 2 ||
          task.position.y > currentCameraChunk.y + 7 + 2) {
        {
          std::lock_guard<std::mutex> lock(loadingMutex);
          chunksLoading.erase(task.position);
          chunksProcessed.insert(task.position);
        }
        return;
      }

      Chunk *chunk = new Chunk(task.position);
      bool isEmpty = true;

      int heightMap[CHUNK_WIDTH][CHUNK_DEPTH];
      for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
          float worldX = x + chunk->chunkPosition.x * CHUNK_WIDTH;
          float worldZ = z + chunk->chunkPosition.z * CHUNK_DEPTH;
          float perlinValue = glm::perlin(glm::vec2(worldX, worldZ) * 0.01f);
          heightMap[x][z] =
              static_cast<int>(((perlinValue + 1.0f) / 2.0f) * MAX_HEIGHT);
        }
      }

      for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
          int height = heightMap[x][z];
          for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            int worldY = y + chunk->chunkPosition.y * CHUNK_HEIGHT;
            if (worldY <= height) {
              chunk->setVoxel(x, y, z, Voxel::GREEN);
              isEmpty = false;
            }
          }
        }
      }

      if (isEmpty) {
        {
          std::lock_guard<std::mutex> lock(loadingMutex);
          chunksLoading.erase(task.position);
          chunksProcessed.insert(task.position);
        }
        delete chunk;
        return;
      }
      chunk->status = ChunkState::WAITING_FOR_MESH_UPDATE;

      {
        std::unique_lock<std::shared_mutex> lock(chunk_map_mutex);
        chunk_map[task.position] = chunk;
        onChunkLoaded(chunk);
      } // Release unique lock before mesh generation

      // Now generate meshes
      chunk->generateMeshLOD(LOD_0);
      chunk->generateMeshLOD(LOD_1);
      chunk->generateMeshLOD(LOD_2);

      // Queue neighbor updates
      glm::ivec3 pos = chunk->chunkPosition;
      glm::ivec3 neighbors[6] = {
          pos + glm::ivec3(1, 0, 0), pos + glm::ivec3(-1, 0, 0),
          pos + glm::ivec3(0, 1, 0), pos + glm::ivec3(0, -1, 0),
          pos + glm::ivec3(0, 0, 1), pos + glm::ivec3(0, 0, -1)};

      for (const auto &neighborPos : neighbors) {
        {
          std::shared_lock<std::shared_mutex> lock(chunk_map_mutex);
          auto it = chunk_map.find(neighborPos);
          if (it != chunk_map.end() && it->second != nullptr) {
            it->second->setLODMeshNeedsUpdate(it->second->currentLOD);
            it->second->status = ChunkState::WAITING_FOR_MESH_UPDATE;
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(loadingMutex);
        chunksLoading.erase(task.position);
        chunksLoaded.insert(task.position);
        chunksProcessed.insert(task.position);
      }
    });
  }

  if (tasksSubmitted > 0 && tasksSubmitted < tasksToLoad.size()) {
    std::cout << "Queue limited: submitted " << tasksSubmitted << " of "
              << tasksToLoad.size() << " pending chunks" << std::endl;
  }
}

int WorldManager::getLoadedChunkCount() const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(loadingMutex));
  return chunksLoaded.size();
}

int WorldManager::getLoadingChunkCount() const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(loadingMutex));
  return chunksLoading.size();
}

void WorldManager::onChunkLoaded(Chunk *chunk) {
  // std::unique_lock<std::shared_mutex> lock(chunk_map_mutex);

  glm::ivec3 pos = chunk->chunkPosition;

  const glm::ivec3 offsets[6] = {
      glm::ivec3(1, 0, 0),  // NEIGHBOR_POS_X
      glm::ivec3(-1, 0, 0), // NEIGHBOR_NEG_X
      glm::ivec3(0, 1, 0),  // NEIGHBOR_POS_Y
      glm::ivec3(0, -1, 0), // NEIGHBOR_NEG_Y
      glm::ivec3(0, 0, 1),  // NEIGHBOR_POS_Z
      glm::ivec3(0, 0, -1)  // NEIGHBOR_NEG_Z
  };

  // Opposite directions for bidirectional linking
  const int opposite[6] = {1, 0, 3, 2, 5, 4};

  // Link this chunk to its neighbors (and vice versa)
  for (int dir = 0; dir < 6; ++dir) {
    glm::ivec3 neighborPos = pos + offsets[dir];
    auto it = chunk_map.find(neighborPos);

    if (it != chunk_map.end() && it->second != nullptr) {
      Chunk *neighbor = it->second;

      // Bidirectional linking
      chunk->setNeighbor(dir, neighbor);
      neighbor->setNeighbor(opposite[dir], chunk);

      // Both chunks need to remesh now
      // Their interior faces should become hidden
      neighbor->setLODUpdateAll();
      neighbor->status = ChunkState::WAITING_FOR_MESH_UPDATE;
    }
  }
}

void WorldManager::unloadChunk(const glm::ivec3 &pos) {
  Chunk *chunkToDelete = nullptr;

  {
    std::unique_lock<std::shared_mutex> lock(chunk_map_mutex);

    auto it = chunk_map.find(pos);
    if (it == chunk_map.end() || it->second == nullptr) {
      return; // Already unloaded
    }

    chunkToDelete = it->second;

    // PHASE 1: Sever all connections BEFORE deleting
    const glm::ivec3 offsets[6] = {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
                                   glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
                                   glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)};
    const int opposite[6] = {1, 0, 3, 2, 5, 4};

    // Tell all neighbors to forget about this chunk
    for (int dir = 0; dir < 6; ++dir) {
      glm::ivec3 neighborPos = pos + offsets[dir];
      auto neighborIt = chunk_map.find(neighborPos);

      if (neighborIt != chunk_map.end() && neighborIt->second != nullptr) {
        Chunk *neighbor = neighborIt->second;

        // Clear the neighbor's pointer to THIS chunk
        neighbor->clearNeighbor(opposite[dir]);

        // Neighbor now has an exposed face - remesh it
        neighbor->setLODUpdateAll();
        neighbor->status = ChunkState::WAITING_FOR_MESH_UPDATE;
      }
    }

    // Clear this chunk's neighbor pointers (defensive)
    chunkToDelete->clearAllNeighbors();

    // Remove from map
    chunk_map.erase(it);
    chunksLoaded.erase(pos);
  }

  // PHASE 2: Queue for deletion AFTER releasing lock and severing all links
  // We defer actual deletion to avoid use-after-free when main thread is
  // iterating over chunks.
  chunkToDelete->status = ChunkState::MARKED_FOR_DELETION;
  {
    std::lock_guard<std::mutex> del_lock(delete_queue_mutex);
    chunksToDelete.push_back(chunkToDelete);
  }
}

void WorldManager::cleanUpDeletedChunks() {
  std::vector<Chunk *> toDelete;
  {
    std::lock_guard<std::mutex> lock(delete_queue_mutex);
    toDelete = std::move(chunksToDelete);
    chunksToDelete.clear();
  }

  for (Chunk *chunk : toDelete) {
    delete chunk;
  }
}