#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.hpp"
//#include "ObjectSun.hpp"
#include "object.hpp"
#include "camera.hpp"
#include "voxel.hpp"
#include "chunk.hpp"
#include "threadPool.hpp"
#include "frustum.hpp"
#include "worldManager.hpp"
#include "lodManager.hpp"

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel Engine", NULL, NULL);
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
    ThreadPool threadPool(4);
    WorldManager worldManager(12);
    LODManager lodManager;

    // Setup OpenGL objects
    std::vector<unsigned int> indices = {};
    std::vector<VertexAttribute> VoxelAttributes = {
        VertexAttribute(GL_UNSIGNED_INT, 1, false),
        VertexAttribute(GL_UNSIGNED_INT, 1, false)
    };

    std::vector<VertexAttribute> SunAttributes = {
        VertexAttribute(GL_FLOAT, 3, false)
    };

    std::vector<ChunkVertex> initialVertices;
    Object voxelObj(initialVertices, VoxelAttributes, &indices, GL_POINTS);

    std::vector<float> sunVertices = {
        //position
        0.5f,  0.5f, 0.5f,  //  front top right
        0.5f, -0.5f, 0.5f, // front bottom right
        -0.5f, -0.5f, 0.5f,  // front bottom left
        -0.5f,  0.5f, 0.5f, // front top left
        0.5f,  0.5f, -0.5f,  //  back top right
        0.5f, -0.5f, -0.5f,  // back bottom right
        -0.5f, -0.5f, -0.5f, // back bottom left
        -0.5f,  0.5f, -0.5f // back top left

        
    };
    

    std::vector<unsigned int> sunIndices = {
      0, 1, 3,// 1st Triangle
      1, 2, 3,// 2st Triangle
      4, 5, 7,// 3st Triangle
      5, 6, 7,// 4st Triangle
      4,5,1,// 5st Triangle
      0,1,4,// 6st Triangle
      6,7,2,// 7st Triangle
      3,4,7,// 8st Triangle
      1,2,6,// 9st Triangle
      5,6,1,// 10st Triangle
      0,7,4,// 11st Triangle
      0,7,3,// 12st Triangle
      

    };

    Object sunObj(sunVertices, SunAttributes, &sunIndices, GL_TRIANGLES);
   

    Shader baseShader("source/base.vs", "source/base.gs", "source/base.fs");
    Shader SunShader("source/light.vs",  "source/light.fs");
   


    // OpenGL state
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_BACK);
    //glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    
    // Create frustum for culling
    Frustum frustum;
    
    std::cout << "Window opened! Chunks will stream in around you..." << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move horizontally" << std::endl;
    std::cout << "  QE - Move up/down" << std::endl;
    std::cout << "  TAB - Toggle wireframe" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    // Start world management thread
    worldManager.start(&threadPool);

    // Stats tracking
    //int frameCount = 0;
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
        glm::mat4 model = glm::mat4(1.0f);

   
        
        // Update frustum
        glm::mat4 projectionView = projection * view;
        frustum.update(projectionView);

        // Update world manager with camera position
        glm::vec3 cameraPos = camera.getPosition();
        worldManager.updateCameraPosition(cameraPos);

        // Pre-allocate vertex buffer
        std::vector<ChunkVertex> frameVertices;
        frameVertices.reserve(10000);
        
        chunksRendered = 0;
        chunksCulled = 0;

        double slowTime = glfwGetTime() * 0.01f;
        // lighting
        glm::vec3 lightPos(static_cast<float>(-600.0f*sin(slowTime * 2.0))+cameraPos.x, static_cast<float>(400.0f*sin(slowTime * 1.0)-50)+cameraPos.y, cameraPos.z);

        
        // Set shader uniforms once
        baseShader.use();
        glm::vec3 lightColor;
        lightColor.x = static_cast<float>(.4f*sin(slowTime * 1.0)+0.65f);
        lightColor.y = static_cast<float>(.4f*sin(slowTime * 1.0)+0.65f);
        lightColor.z = static_cast<float>(.4f*sin(slowTime * 1.0)+0.65f);
        glm::vec3 diffuseColor = lightColor * glm::vec3(1.0f); // decrease the influence
        glm::vec3 ambientColor = diffuseColor * glm::vec3(1.0f); // low influence
        baseShader.setVec3("lightColor", lightColor);
        baseShader.setVec3("light.ambient", ambientColor);
        baseShader.setVec3("light.diffuse", diffuseColor);
        baseShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
        baseShader.setVec3("light.position", lightPos);
        
      

        //glm::f32 shininess;
        //baseShader.setFloat("material.shininess", shininess);

        baseShader.setMat4("projection", projection);
        baseShader.setMat4("view", view);
        
        // world transformation
        baseShader.setVec3("viewPos", camera.position);
        glm::mat4 sunmodel = glm::mat4(1.0f);
        baseShader.setMat4("model", sunmodel);
        //Flashlight code


        static float totalCopyTime = 0.0f;
        static float lockTime = 0.0f;
        
        // Process chunks directly without copying the entire map
        {
            auto lockStart = std::chrono::high_resolution_clock::now();
            std::shared_lock<std::shared_mutex> lock(worldManager.getChunkMapMutex());
            auto lockEnd = std::chrono::high_resolution_clock::now();
            lockTime += std::chrono::duration<float, std::milli>(lockEnd - lockStart).count();
            
            ChunkMap& chunkMap = worldManager.getChunkMap();
            
            for (auto& pair : chunkMap) {
                Chunk* chunk = pair.second;
                glm::vec3 chunkWorldPos = glm::vec3(chunk->chunkPosition) * 16.0f;
                
                if (!frustum.isChunkVisible(chunkWorldPos, 16.0f)) {
                    chunksCulled++;
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
                uint32_t chunkOffset = packChunkOffset(chunk->chunkPosition, chunk->currentLOD);

                // Pack vertices directly into frame buffer
                size_t startSize = frameVertices.size();
                frameVertices.resize(startSize + chunkVertices.size());

                // Use pointer arithmetic for faster copying
                ChunkVertex* dest = &frameVertices[startSize];
                const Voxel::PackedVoxel* src = chunkVertices.data();
                size_t count = chunkVertices.size();
                
                for (size_t i = 0; i < count; ++i) {
                    dest[i].packedData = src[i];
                    dest[i].chunkOffset = chunkOffset;
                }
            }
            auto lockEnd2 = std::chrono::high_resolution_clock::now();
            totalCopyTime += std::chrono::duration<float, std::milli>(lockEnd2 - lockEnd).count();
        }

        // Render all visible chunks
        static float uploadTime = 0.0f;
        //Put ! infront of if statement to toggle terrain
        if (!frameVertices.empty()) {
            auto startTime = std::chrono::high_resolution_clock::now();
            voxelObj.bindVertices(frameVertices.data(), frameVertices.size() * sizeof(ChunkVertex), GL_DYNAMIC_DRAW);
            voxelObj.vertexCount = frameVertices.size();
            voxelObj.draw();
            auto endTime = std::chrono::high_resolution_clock::now();
            uploadTime += std::chrono::duration<float, std::milli>(endTime - startTime).count();
        }

        SunShader.use();
        SunShader.setMat4("projection", projection);
        SunShader.setMat4("view", view);
        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(25.0f)); 
        glm::vec3 lighter;
        lighter.x = static_cast<float>(.4f*sin(slowTime * 1.0)+0.65f);
        lighter.y = static_cast<float>(.4f*sin(slowTime * 1.0)+0.5f);
        lighter.z = static_cast<float>(.05f*sin(slowTime * 1.0)+0.1f);
        SunShader.setVec3("lighter", lighter);
        SunShader.setMat4("model", model);

        sunObj.bindVertices(sunVertices.data(), sunVertices.size() * (sizeof(glm::vec3)), GL_DYNAMIC_DRAW);
        sunObj.bindVertexArray();
        glDrawElements(GL_TRIANGLES, sunIndices.size() * sizeof(glm::vec3), GL_UNSIGNED_INT, 0);


        

        


        // Print stats every second
        if (currentFrame - lastInfoTime > 1.0f) {
            lastInfoTime = currentFrame;
            float fps = 1.0f / deltaTime;
            std::cout << "FPS: " << fps 
                      << " | Chunks loaded: " << worldManager.getLoadedChunkCount()
                      << " | Rendered: " << chunksRendered
                      << " | Frustum culled: " << chunksCulled
                      << " | Vertices: " << frameVertices.size() 
                      << std::endl;
            std::cout << "Current Chunks in queue: " << worldManager.getLoadingChunkCount() << std::endl;
            float percentPerFrame = ((uploadTime / fps) / (1000.0f / fps)) * 100.0f;
            std::cout << "Vertex upload time: " << uploadTime << " ms | " << percentPerFrame << "%" << std::endl;
            std::cout << "Time spent copying vertex data: " << totalCopyTime << " ms" << std::endl;
            std::cout << "Time spent acquiring lock: " << lockTime << " ms" << std::endl;
            uploadTime = 0.0f;
            totalCopyTime = 0.0f;
            lockTime = 0.0f;
        }
        //frameCount++;
 
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


    // Camera movement
    float cameraSpeed = 75.0f * deltaTime;
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