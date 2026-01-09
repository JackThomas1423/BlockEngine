#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.hpp"
#include "object.hpp"
#include "camera.hpp"
#include "voxel.hpp"
#include "chunk.hpp"
#include "threadPool.hpp"
#include "frustum.hpp"
#include "worldManager.hpp"
#include "lodManager.hpp"
#include "occlusionCuller.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

// Settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

// Global state
Camera camera(glm::vec3(0.0f, 64.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 70.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;
OcclusionCuller* g_occlusionCuller = nullptr;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);

int main()
{
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel Engine - With Occlusion Culling", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Initialize systems
    ThreadPool threadPool(8);
    WorldManager worldManager(32);
    LODManager lodManager;
    OcclusionCuller occlusionCuller;
    BoundingBoxRenderer bboxRenderer;
    
    // Set global pointer for keyboard toggle
    g_occlusionCuller = &occlusionCuller;

    // Setup OpenGL objects
    std::vector<unsigned int> indices = {};
    std::vector<VertexAttribute> attributes = {
        VertexAttribute(GL_UNSIGNED_INT, 1, false),
        VertexAttribute(GL_UNSIGNED_INT, 1, false)
    };

    std::vector<ChunkVertex> initialVertices;
    Object obj(initialVertices, attributes, &indices, GL_POINTS);

    Shader baseShader("source/base.vs", "source/base.gs", "source/base.fs");

    // OpenGL state
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    
    // Create frustum for culling
    Frustum frustum;
    
    std::cout << "Window opened! Chunks will stream in around you..." << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move horizontally" << std::endl;
    std::cout << "  QE - Move up/down" << std::endl;
    std::cout << "  TAB - Toggle wireframe" << std::endl;
    std::cout << "  O - Toggle occlusion culling" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    // Start world management thread
    worldManager.start(&threadPool);

    // Stats tracking
    int frameCount = 0;
    float lastInfoTime = 0.0f;
    int chunksRendered = 0;
    int chunksCulled = 0;

    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);

        // Clear screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update camera matrices
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix((float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);

        // Update frustum
        glm::mat4 projectionView = projection * view;
        frustum.update(projectionView);

        // Update world manager with camera position
        glm::vec3 cameraPos = camera.getPosition();
        worldManager.updateCameraPosition(cameraPos);

        // Begin occlusion testing for this frame
        occlusionCuller.beginFrame();

        // PASS 1: Frustum culling and collect potentially visible chunks
        std::vector<Chunk*> potentiallyVisibleChunks;
        chunksCulled = 0;

        {
            std::lock_guard<std::mutex> lock(worldManager.getChunkMapMutex());
            ChunkMap& chunkMap = worldManager.getChunkMap();
            
            for (auto& pair : chunkMap) {
                Chunk* chunk = pair.second;
                glm::vec3 chunkWorldPos = glm::vec3(chunk->chunkPosition) * 16.0f;
                
                // Frustum culling (fast CPU test)
                if (!frustum.isChunkVisible(chunkWorldPos, 16.0f)) {
                    chunksCulled++;
                    continue;
                }
                
                potentiallyVisibleChunks.push_back(chunk);
            }
        }

        // Sort front-to-back for better occlusion
        std::sort(potentiallyVisibleChunks.begin(), potentiallyVisibleChunks.end(),
            [&cameraPos](Chunk* a, Chunk* b) {
                glm::vec3 posA = glm::vec3(a->chunkPosition) * 16.0f + glm::vec3(8.0f);
                glm::vec3 posB = glm::vec3(b->chunkPosition) * 16.0f + glm::vec3(8.0f);
                float distA = glm::distance(posA,cameraPos);
                float distB = glm::distance(posB,cameraPos);
                return distA > distB;
            });

        // PASS 2: Render visible chunks based on PREVIOUS frame's occlusion results
        std::vector<ChunkVertex> frameVertices;
        frameVertices.reserve(10000);
        
        chunksRendered = 0;

        // Set shader uniforms for voxel rendering
        baseShader.use();
        baseShader.setMat4("projection", projection);
        baseShader.setMat4("view", view);

        // Collect visible chunks and their geometry
        for (Chunk* chunk : potentiallyVisibleChunks) {
            glm::vec3 chunkWorldPos = glm::vec3(chunk->chunkPosition) * 16.0f;
            
            // Test visibility using PREVIOUS frame's results
            bool isVisible = occlusionCuller.testChunkVisibility(chunk, chunkWorldPos);
            
            if (!isVisible) {
                continue;
            }
            
            chunksRendered++;
            
            // Calculate and update LOD
            LODLevel newLOD = lodManager.calculateLOD(chunkWorldPos, cameraPos, chunk->getLODLevel());
            
            if (newLOD != chunk->getLODLevel()) {
                chunk->setLODLevel(newLOD);
            }
    
            // Get mesh data for current LOD
            const std::vector<Voxel::PackedVoxel>& chunkVertices = chunk->getMeshData(newLOD);
    
            // Pack vertices for rendering
            for (const Voxel::PackedVoxel& pv : chunkVertices) {
                ChunkVertex cv;
                cv.packedData = pv;
                cv.chunkOffset = packChunkOffset(chunk->chunkPosition, chunk->currentLOD);
                frameVertices.push_back(cv);
            }
        }

        // Render all visible chunks - this builds the depth buffer
        if (!frameVertices.empty()) {
            obj.bindVertices(frameVertices.data(), frameVertices.size() * sizeof(ChunkVertex), GL_DYNAMIC_DRAW);
            obj.vertexCount = frameVertices.size();
            obj.draw();
        }

        // PASS 3: Now test occlusion queries for NEXT frame using the depth buffer we just built
        // Disable color writes and depth writes - only test against existing depth
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);  // Test against existing depth
        
        for (Chunk* chunk : potentiallyVisibleChunks) {
            glm::vec3 chunkWorldPos = glm::vec3(chunk->chunkPosition) * 16.0f;
            
            // Submit query for next frame (tests against current depth buffer)
            occlusionCuller.submitOcclusionQuery(chunk, chunkWorldPos, projectionView, &bboxRenderer);
        }
        
        // Restore state
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // End occlusion testing
        occlusionCuller.endFrame();

        // Print stats every second
        if (currentFrame - lastInfoTime > 1.0f) {
            lastInfoTime = currentFrame;
            std::cout << "FPS: " << (1.0f / deltaTime) 
                      << " | Chunks loaded: " << worldManager.getLoadedChunkCount()
                      << " | Rendered: " << chunksRendered
                      << " | Frustum culled: " << chunksCulled
                      << " | Occlusion culled: " << occlusionCuller.getCulledChunks()
                      << " | Occlusion queries: " << occlusionCuller.getQueriesThisFrame()
                      << " | Vertices: " << frameVertices.size() 
                      << " | OC: " << (occlusionCuller.isEnabled() ? "ON" : "OFF")
                      << std::endl;
        }
        frameCount++;

        // Render all visible chunks
        if (!frameVertices.empty()) {
            obj.bindVertices(frameVertices.data(), frameVertices.size() * sizeof(ChunkVertex), GL_DYNAMIC_DRAW);
            obj.vertexCount = frameVertices.size();
            obj.draw();
        }
 
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    worldManager.stop();
    
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle wireframe mode
    static bool tabWasPressed = false;
    static bool wireframe = false;
    int tabState = glfwGetKey(window, GLFW_KEY_TAB);
    if (tabState == GLFW_PRESS && !tabWasPressed) {
        wireframe = !wireframe;
        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        tabWasPressed = true;
    }
    if (tabState == GLFW_RELEASE) tabWasPressed = false;

    // Toggle occlusion culling
    static bool oWasPressed = false;
    
    int oState = glfwGetKey(window, GLFW_KEY_O);
    if (oState == GLFW_PRESS && !oWasPressed && g_occlusionCuller) {
        bool enabled = !g_occlusionCuller->isEnabled();
        g_occlusionCuller->setEnabled(enabled);
        std::cout << "Occlusion culling: " << (enabled ? "ON" : "OFF") << std::endl;
        oWasPressed = true;
    }
    if (oState == GLFW_RELEASE) oWasPressed = false;

    // Camera movement
    float cameraSpeed = 50.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.translate(cameraSpeed * camera.getFront());
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.translate(-cameraSpeed * camera.getFront());
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.translate(-glm::normalize(glm::cross(camera.getFront(), camera.getUp())) * cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.translate(glm::normalize(glm::cross(camera.getFront(), camera.getUp())) * cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.translate(-cameraSpeed * camera.getUp());
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.translate(cameraSpeed * camera.getUp());
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    static bool firstMouse = true;
    static float lastX = SCR_WIDTH / 2.0f;
    static float lastY = SCR_HEIGHT / 2.0f;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    static float yaw = -90.0f;
    static float pitch = 0.0f;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camera.setFront(glm::normalize(front));
}