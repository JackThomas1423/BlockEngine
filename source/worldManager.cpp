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

void WorldManager::gameLoop() {
    const float UPDATE_INTERVAL = 0.05f;
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
            queueChunksForLoading(cameraChunk, cameraPos);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void WorldManager::unloadDistantChunks(const glm::ivec3& cameraChunk) {
    std::vector<glm::ivec3> chunksToUnload;
    {
        std::lock_guard<std::mutex> lock(chunk_map_mutex);
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
        std::lock_guard<std::mutex> lock(chunk_map_mutex);
        auto it = chunk_map.find(pos);
        if (it != chunk_map.end()) {
            delete it->second;
            chunk_map.erase(it);
        }
        
        std::lock_guard<std::mutex> loadLock(loadingMutex);
        chunksLoaded.erase(pos);
    }
    
    if (!chunksToUnload.empty()) {
        std::cout << "Unloaded " << chunksToUnload.size() << " chunks" << std::endl;
    }
}

void WorldManager::queueChunksForLoading(const glm::ivec3& cameraChunk, const glm::vec3& cameraPos) {
    std::vector<ChunkLoadTask> tasksToLoad;
    int maxTerrainChunkY = (MAX_HEIGHT / CHUNK_HEIGHT) + 1;
    
    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
        for (int y = -4; y <= 7; ++y) {
            for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
                glm::ivec3 chunkPos = cameraChunk + glm::ivec3(x, y, z);
                
                if (chunkPos.y > maxTerrainChunkY) continue;
                
                {
                    std::lock_guard<std::mutex> lock(loadingMutex);
                    if (chunksLoaded.count(chunkPos) > 0 || 
                        chunksLoading.count(chunkPos) > 0) {
                        continue;
                    }
                    chunksLoading.insert(chunkPos);
                }
                
                glm::vec3 chunkWorldPos = glm::vec3(chunkPos) * 16.0f + glm::vec3(8.0f);
                float distance = glm::length(chunkWorldPos - cameraPos);
                tasksToLoad.push_back({chunkPos, distance});
            }
        }
    }
    
    std::sort(tasksToLoad.begin(), tasksToLoad.end());
    
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
                }
                pendingTaskCount--;
                delete chunk;
                return;
            }
            
            {
                std::lock_guard<std::mutex> lock(chunk_map_mutex);
                chunk_map[task.position] = chunk;
                chunk->map_ptr = &chunk_map;
            }
            
            chunk->createMeshData();
            
            {
                std::lock_guard<std::mutex> lock(loadingMutex);
                chunksLoading.erase(task.position);
                chunksLoaded.insert(task.position);
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