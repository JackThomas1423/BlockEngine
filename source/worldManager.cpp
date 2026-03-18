#include "worldManager.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

WorldManager::WorldManager(int renderDistance)
    : RENDER_DISTANCE(renderDistance) {
  initLoadingOffsets();
}

WorldManager::~WorldManager() {
  stop();
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

void WorldManager::initLoadingOffsets() {
  for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
    for (int y = -4; y <= 7; ++y) {
      for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
        loadingOffsets.push_back(glm::ivec3(x, y, z));
      }
    }
  }

  std::sort(loadingOffsets.begin(), loadingOffsets.end(),
            [](const glm::ivec3 &a, const glm::ivec3 &b) {
              float distA = a.x * a.x + a.y * a.y + a.z * a.z;
              float distB = b.x * b.x + b.y * b.y + b.z * b.z;
              return distA < distB;
            });
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

  for (const auto &pos : chunksToUnload) {
    unloadChunk(pos);
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
  for (const auto &offset : loadingOffsets) {
    glm::ivec3 chunkPos = cameraChunk + offset;

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

      if (task.position.y > 0) {
        float persistence = 0.5f;
        float lacunarity = 2.0f;
        int octaves = 4;

        float initialFrequency = 0.005f;

        int heightMap [CHUNK_WIDTH][CHUNK_DEPTH];
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
          for (int z = 0; z < CHUNK_DEPTH; ++z) {
            float worldX = x + chunk->chunkPosition.x * CHUNK_WIDTH;
            float worldZ = z + chunk->chunkPosition.z * CHUNK_DEPTH;
            float amplitude = 1.0f;
            float totalNoise = 0.0f;
            float amplitudeSum = 0.0f;
            float frequency = initialFrequency;

            for (int i = 0; i < octaves; ++i) {
              float perlinValue = glm::perlin(glm::vec2(worldX, worldZ) * frequency);
              totalNoise += perlinValue * amplitude;
              amplitudeSum += amplitude;

              amplitude *= persistence;
              frequency *= lacunarity;
            }

            float normalizedNoise = (amplitudeSum > 0.0f) ? (totalNoise / amplitudeSum) : 0.0f;
            normalizedNoise = glm::clamp(normalizedNoise, -1.0f, 1.0f);

            heightMap[x][z] = static_cast<int>(((normalizedNoise + 1.0f) / 2.0f) * MAX_HEIGHT);
          }
        }


        for (int x = 0; x < CHUNK_WIDTH; ++x){
          for (int z = 0; z < CHUNK_DEPTH; ++z){
            int height = heightMap[x][z];
            for (int y = 0; y < CHUNK_HEIGHT; ++y){
              int worldY = y + chunk->chunkPosition.y * CHUNK_HEIGHT;
              if (worldY < height){
                chunk->setVoxel(x, y, z, Voxel::GREEN);//Voxel::GREEN); // Solid voxel
                isEmpty = false;
              }
            }
          }
        }

      } else {
        float noiseMap[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
          for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
              float worldX = x + chunk->chunkPosition.x * CHUNK_WIDTH;
              float worldY = y + chunk->chunkPosition.y * CHUNK_HEIGHT;
              float worldZ = z + chunk->chunkPosition.z * CHUNK_DEPTH;
              float noisevalue = glm::perlin(glm::vec3(worldX, worldY, worldZ) * 0.01f);
                          
              noiseMap[x][y][z] = glm::clamp((noisevalue + 1.0f) / 2.0f, 0.0f, 1.0f);
            }
          }
        }
        
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
          for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            float gradient = 0.0f;
            if (task.position.y > -1) {
              gradient = y * 0.02f;
            } else if (task.position.y > -2){
              gradient = (y - 16.0f) * 0.01f;
            }
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
              float density = noiseMap[x][y][z];
              if (task.position.y > -2) {
                if (density > 0.4f - gradient) {
                  chunk->setVoxel(x, y, z, Voxel::GREEN); // Solid voxel
                  isEmpty = false;
                } else {
                  chunk->setVoxel(x, y, z, 0); // Air voxel
                }
              } else {
                if (density > 0.565f){
                  chunk->setVoxel(x, y, z, Voxel::GREEN); // Solid voxel
                  isEmpty = false;
                } else {
                  chunk->setVoxel(x, y, z, 0); // Air voxel
                }
              }
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
      }

      // Add to both parallel arrays atomically under one lock.
      {
        std::unique_lock<std::shared_mutex> lock(activeChunksMutex);
        activeChunks.push_back(chunk);
        activeChunkWorldPos.push_back(glm::vec3(chunk->chunkPosition) * 16.0f);
      }

      chunk->generateMesh();

      // Queue neighbor updates
      glm::ivec3 pos = chunk->chunkPosition;
      glm::ivec3 neighborOffsets[6] = {
          pos + glm::ivec3(1, 0, 0), pos + glm::ivec3(-1, 0, 0),
          pos + glm::ivec3(0, 1, 0), pos + glm::ivec3(0, -1, 0),
          pos + glm::ivec3(0, 0, 1), pos + glm::ivec3(0, 0, -1)};

      for (const auto &neighborPos : neighborOffsets) {
        {
          std::shared_lock<std::shared_mutex> lock(chunk_map_mutex);
          auto it = chunk_map.find(neighborPos);
          if (it != chunk_map.end() && it->second != nullptr) {
            it->second->setMeshNeedsUpdate();
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

void WorldManager::queueMeshUpdate(Chunk *chunk) {
  chunk->status = ChunkState::GENERATING;
  updatePool->enqueue([chunk]() {
    chunk->generateMesh();
  });
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
  glm::ivec3 pos = chunk->chunkPosition;

  const glm::ivec3 offsets[6] = {
      glm::ivec3(1, 0, 0),  // NEIGHBOR_POS_X
      glm::ivec3(-1, 0, 0), // NEIGHBOR_NEG_X
      glm::ivec3(0, 1, 0),  // NEIGHBOR_POS_Y
      glm::ivec3(0, -1, 0), // NEIGHBOR_NEG_Y
      glm::ivec3(0, 0, 1),  // NEIGHBOR_POS_Z
      glm::ivec3(0, 0, -1)  // NEIGHBOR_NEG_Z
  };

  const int opposite[6] = {1, 0, 3, 2, 5, 4};

  for (int dir = 0; dir < 6; ++dir) {
    glm::ivec3 neighborPos = pos + offsets[dir];
    auto it = chunk_map.find(neighborPos);

    if (it != chunk_map.end() && it->second != nullptr) {
      Chunk *neighbor = it->second;

      chunk->setNeighbor(dir, neighbor);
      neighbor->setNeighbor(opposite[dir], chunk);

      neighbor->setMeshNeedsUpdate();
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
      return;
    }

    chunkToDelete = it->second;

    const glm::ivec3 offsets[6] = {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
                                   glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
                                   glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)};
    const int opposite[6] = {1, 0, 3, 2, 5, 4};

    for (int dir = 0; dir < 6; ++dir) {
      glm::ivec3 neighborPos = pos + offsets[dir];
      auto neighborIt = chunk_map.find(neighborPos);

      if (neighborIt != chunk_map.end() && neighborIt->second != nullptr) {
        Chunk *neighbor = neighborIt->second;

        neighbor->clearNeighbor(opposite[dir]);

        neighbor->setMeshNeedsUpdate();
        neighbor->status = ChunkState::WAITING_FOR_MESH_UPDATE;
      }
    }

    chunkToDelete->clearAllNeighbors();

    chunk_map.erase(it);
    chunksLoaded.erase(pos);
  }

  {
    // Remove from both parallel arrays using the swap-and-pop idiom.
    // This is O(n) for the find but O(1) for the removal itself,
    // matching the original behaviour while keeping arrays in sync.
    std::unique_lock<std::shared_mutex> lock(activeChunksMutex);
    auto itVec = std::find(activeChunks.begin(), activeChunks.end(), chunkToDelete);
    if (itVec != activeChunks.end()) {
      size_t idx = std::distance(activeChunks.begin(), itVec);

      // Swap-and-pop both arrays together so indices stay aligned.
      activeChunks[idx]        = activeChunks.back();
      activeChunkWorldPos[idx] = activeChunkWorldPos.back();
      activeChunks.pop_back();
      activeChunkWorldPos.pop_back();
    }
  }

  chunkToDelete->markedForDeletion.store(true, std::memory_order_release);
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