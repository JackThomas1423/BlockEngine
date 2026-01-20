#include "worldManager.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>

WorldManager::WorldManager(int renderDistance) 
    : RENDER_DISTANCE(renderDistance) {}

WorldManager::~WorldManager() {
    stop();
    for (auto& pair : chunk_map) {
        delete pair.second;
    }
}

void WorldManager::start(ThreadPool* threadPool) {
    pool = threadPool;
    running = true;
    gameThread = std::thread(&WorldManager::gameLoop, this);
}

void WorldManager::stop() {
    running = false;
    if (gameThread.joinable()) {
        gameThread.join();
    }
}

void WorldManager::updateCameraPosition(const glm::vec3& cameraPosition) {
    std::lock_guard<std::mutex> lock(cameraPosMutex);
    currentCameraPosition = cameraPosition;
}

glm::vec3 WorldManager::getCurrentCameraPosition() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(cameraPosMutex));
    return currentCameraPosition;
}

void WorldManager::gameLoop() {
    const float UPDATE_INTERVAL = 0.01f;
    auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastUpdateTime).count();
        
        if (deltaTime >= UPDATE_INTERVAL) {
            lastUpdateTime = currentTime;
            
            // Get camera position from main thread
            glm::vec3 cameraPos;
            {
                std::lock_guard<std::mutex> lock(cameraPosMutex);
                cameraPos = currentCameraPosition;
            }
            
            glm::ivec3 cameraChunk = glm::ivec3(
                floor(cameraPos.x / 16.0f),
                floor(cameraPos.y / 16.0f),
                floor(cameraPos.z / 16.0f)
            );
            
            unloadDistantChunks(cameraChunk);
            pruneOutOfRangeLoadingChunks(cameraChunk);
            queueChunksForLoading(cameraChunk, cameraPos);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void WorldManager::unloadDistantChunks(const glm::ivec3& cameraChunk) {
    std::vector<glm::ivec3> chunksToUnload;
    {
        std::lock_guard<std::shared_mutex> lock(chunk_map_mutex);
        for (auto& pair : chunk_map) {
            glm::ivec3 chunkPos = pair.first;
            glm::ivec3 diff = chunkPos - cameraChunk;
            
            int horizontalDist = std::max(std::abs(diff.x), std::abs(diff.z));
            
            if (horizontalDist > RENDER_DISTANCE + 2 || 
                chunkPos.y < cameraChunk.y - 4 - 2 || 
                chunkPos.y > cameraChunk.y + 7 + 2) {
                chunksToUnload.push_back(chunkPos);
            }
        }
    }
    
    for (const auto& pos : chunksToUnload) {
        std::lock_guard<std::shared_mutex> lock(chunk_map_mutex);
        auto it = chunk_map.find(pos);
        if (it != chunk_map.end()) {
            delete it->second;
            chunk_map.erase(it);
        }
        
        std::lock_guard<std::mutex> loadLock(loadingMutex);
        chunksLoaded.erase(pos);
        chunksProcessed.erase(pos);
    }
    
    /*if (!chunksToUnload.empty()) {
        std::cout << "Unloaded " << chunksToUnload.size() << " chunks" << std::endl;
    }*/
}

void WorldManager::pruneOutOfRangeLoadingChunks(const glm::ivec3& cameraChunk) {
    std::vector<glm::ivec3> chunksToPrune;
    
    {
        std::lock_guard<std::mutex> lock(loadingMutex);
        for (const auto& chunkPos : chunksLoading) {
            glm::ivec3 diff = chunkPos - cameraChunk;
            int horizontalDist = std::max(std::abs(diff.x), std::abs(diff.z));
            
            if (horizontalDist > RENDER_DISTANCE + 2 || 
                chunkPos.y < cameraChunk.y - 4 - 2 || 
                chunkPos.y > cameraChunk.y + 7 + 2) {
                chunksToPrune.push_back(chunkPos);
            }
        }
        
        for (const auto& pos : chunksToPrune) {
            chunksLoading.erase(pos);
            chunksProcessed.insert(pos); // Mark as processed so it won't be re-queued
        }
    }
    
    if (!chunksToPrune.empty()) {
        std::cout << "Pruned " << chunksToPrune.size() << " out-of-range chunks from loading queue" << std::endl;
    }
}

void WorldManager::queueChunksForLoading(const glm::ivec3& cameraChunk, const glm::vec3& cameraPos) {
    std::vector<ChunkLoadTask> tasksToLoad;
    int maxTerrainChunkY = (MAX_HEIGHT / CHUNK_HEIGHT) + 1;

    int totalQueued = 0;
    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
        for (int y = -4; y <= 7; ++y) {
            for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
                glm::ivec3 chunkPos = cameraChunk + glm::ivec3(x, y, z);

                // Skip if above max terrain height
                if (chunkPos.y > maxTerrainChunkY) continue;

                {
                    std::lock_guard<std::mutex> lock(loadingMutex);
                    // Ensure no duplicates are added by checking both loading and loaded sets
                    if (chunksProcessed.find(chunkPos) != chunksProcessed.end()) {
                        continue; // Already processed (loaded, empty, or skipped)
                    }
                    if (chunksLoading.find(chunkPos) != chunksLoading.end()) {
                        continue; // Already in the loading queue
                    }

                    // Add to loading queue only if not already present
                    chunksLoading.insert(chunkPos);
                }

                tasksToLoad.push_back({chunkPos});
                totalQueued++;
            }
        }
    }
    if (totalQueued == 0) return;

    // Sort by distance at queue time (initial sorting)
    std::sort(tasksToLoad.begin(), tasksToLoad.end(), 
        [&cameraPos](const ChunkLoadTask& a, const ChunkLoadTask& b) {
            return a.getDistance(cameraPos) < b.getDistance(cameraPos);
        });

    size_t tasksSubmitted = 0;
    for (const auto& task : tasksToLoad) {
        if (pendingTaskCount >= MAX_PENDING_TASKS) {
            std::lock_guard<std::mutex> lock(loadingMutex);
            chunksLoading.erase(task.position);
            continue;
        }

        pendingTaskCount++;
        tasksSubmitted++;

        pool->enqueue([task, this]() {
            // Dynamically check distance at execution time
            glm::vec3 currentCamPos = getCurrentCameraPosition();
            
            // Optional: Skip if chunk is now too far away
            glm::ivec3 currentCameraChunk = glm::ivec3(
                floor(currentCamPos.x / 16.0f),
                floor(currentCamPos.y / 16.0f),
                floor(currentCamPos.z / 16.0f)
            );
            glm::ivec3 diff = task.position - currentCameraChunk;
            int horizontalDist = std::max(std::abs(diff.x), std::abs(diff.z));
            
            if (horizontalDist > RENDER_DISTANCE + 2 || 
                task.position.y < currentCameraChunk.y - 4 - 2 || 
                task.position.y > currentCameraChunk.y + 7 + 2) {
                // Chunk is now out of range, skip generation
                {
                    std::lock_guard<std::mutex> lock(loadingMutex);
                    chunksLoading.erase(task.position);
                    chunksProcessed.insert(task.position); // Mark as processed so it won't be re-queued
                }
                pendingTaskCount--;
                return;
            }
            
            Chunk* chunk = new Chunk(task.position, nullptr);
            bool isEmpty = true;

            // Terrain generation
            int heightMap[CHUNK_WIDTH][CHUNK_DEPTH];
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                for (int z = 0; z < CHUNK_DEPTH; ++z) {
                    float worldX = x + chunk->chunkPosition.x * CHUNK_WIDTH;
                    float worldZ = z + chunk->chunkPosition.z * CHUNK_DEPTH;
                    float perlinValue = glm::perlin(glm::vec2(worldX, worldZ) * 0.01f);
                    heightMap[x][z] = static_cast<int>(((perlinValue + 1.0f) / 2.0f) * MAX_HEIGHT);
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

            // If chunk is empty, skip adding to map
            if (isEmpty) {
                {
                    std::lock_guard<std::mutex> lock(loadingMutex);
                    chunksLoading.erase(task.position);
                    chunksProcessed.insert(task.position); // Mark as processed so it won't be re-queued
                }
                pendingTaskCount--;
                delete chunk;
                return;
            }

            {
                std::lock_guard<std::shared_mutex> lock(chunk_map_mutex);
                chunk_map[task.position] = chunk;
                chunk->map_ptr = &chunk_map;
            }

            chunk->createMeshData();

            {
                std::lock_guard<std::mutex> lock(loadingMutex);
                chunksLoading.erase(task.position);
                chunksLoaded.insert(task.position);
                chunksProcessed.insert(task.position); // Mark as processed
            }

            pendingTaskCount--;
        });
    }

    if (tasksSubmitted > 0 && tasksSubmitted < tasksToLoad.size()) {
        std::cout << "Queue limited: submitted " << tasksSubmitted 
                  << " of " << tasksToLoad.size() << " pending chunks" << std::endl;
    }
}

int WorldManager::getLoadedChunkCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loadingMutex));
    return chunksLoaded.size();
}

int WorldManager::getLoadingChunkCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loadingMutex));
    return chunksLoading.size();
}