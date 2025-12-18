#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.hpp"
#include "object.hpp"
#include "ubo.hpp"
#include "camera.hpp"
#include "chunk.hpp"

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 70.0f);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
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
    // glad: load all OpenGL function pointers.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::vector<Chunk> global_chunks;
    ChunkMap chunk_map;

    global_chunks.reserve(16 * 16 * 16);

    for (int i = -8; i < 8; ++i) {
        for(int j = -8; j < 8; ++j) {
            for(int k = -8; k < 8; ++k) {
                glm::ivec3 position = glm::ivec3(i, j, k);
                global_chunks.emplace_back(position, &chunk_map);
                chunk_map[position] = &global_chunks.back();

                Chunk& c = global_chunks.back();
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    for (int z = 0; z < CHUNK_DEPTH; ++z) {
                        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                            float height_value = static_cast<int>(((glm::perlin(glm::vec2(x + c.chunkPosition.x * CHUNK_WIDTH, z + c.chunkPosition.z * CHUNK_DEPTH) * 0.01f) + 1.0f) / 2.0f) * MAX_HEIGHT);
                            if (y + c.chunkPosition.y * CHUNK_HEIGHT <= height_value) {
                                c.setVoxel(x, y, z, Voxel::GREEN);
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "Testing for hash collisions..." << std::endl;
    ChunkPositionHash hasher;
    std::unordered_map<size_t, glm::ivec3> hashToPos;
    int collisions = 0;

    for (const auto& pair : chunk_map) {
        glm::ivec3 pos = pair.first;
        size_t hash = hasher(pos);
    
        if (hashToPos.find(hash) != hashToPos.end()) {
            glm::ivec3 existingPos = hashToPos[hash];
            std::cout << "COLLISION: (" << pos.x << "," << pos.y << "," << pos.z << ") and ("
                  << existingPos.x << "," << existingPos.y << "," << existingPos.z 
                  << ") both hash to " << hash << std::endl;
            collisions++;
        } else {
            hashToPos[hash] = pos;
        }
    }

    std::cout << "Total collisions found: " << collisions << std::endl;

    std::vector<ChunkVertex> allVertices;

    unsigned int idCounter = 0;
    for (Chunk& c : global_chunks) {
        std::vector<Voxel::PackedVoxel> chunkVertices = c.createMeshData();
    
        for (const Voxel::PackedVoxel& pv : chunkVertices) {
            ChunkVertex cv;
            cv.packedData = pv;
            cv.offsetID = idCounter;
            allVertices.push_back(cv);
        }
        idCounter++;
    }

    // Info: calculate average data per chunk
    int avgVertices = int((float)allVertices.size() / (float)global_chunks.size());
    std::cout << "Average vertices per chunk: " << avgVertices << std::endl;
    std::cout << "Average of " << (avgVertices * sizeof(ChunkVertex)) << " bytes per chunk." << std::endl;
    std::cout << "Projected memory usage: " << (avgVertices * sizeof(ChunkVertex) * chunk_map.size()) / (1024.0f * 1024.0f) << " MB for " << chunk_map.size() << " chunks." << std::endl;

    std::vector<unsigned int> indices = {};
    std::vector<VertexAttribute> attributes = {
        VertexAttribute(GL_UNSIGNED_INT,  1,  false), // packedData
        VertexAttribute(GL_UNSIGNED_INT,  1,  false)  // offsetID
    };

    Object obj(allVertices, attributes, &indices, GL_POINTS);

    Shader base("source/base.vs","source/base.gs","source/base.fs");

    std::vector<glm::ivec4> chunkOffsets;
    for (const Chunk& c : global_chunks) {
        glm::ivec3 p = c.chunkPosition;
        chunkOffsets.push_back(glm::ivec4(p.x, p.y, p.z, 0));
    }

    GLint maxUBOSize;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);

    int plannedAllocation = sizeof(glm::ivec4) * (16 * 16 * 16);

    std::cout << "Max UBO size: " << maxUBOSize / 1024 << " KB" << std::endl;
    std::cout << "Percent of max UBO used: " << (float)plannedAllocation / (float)maxUBOSize * 100.0f << " %" << std::endl;

    UniformBuffer chunkOffsetUBO(plannedAllocation, GL_STATIC_DRAW); // Binding point 0
    chunkOffsetUBO.update(chunkOffsets);
    chunkOffsetUBO.bindToBindingPoint(0);
    base.setBlockBinding("ChunkOffsets", 0);

    glm::mat4 projection = camera.getProjectionMatrix((float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));

    // render loop
    // -----------

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input
        processInput(window);

        // render

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        view = camera.getViewMatrix();
        projection = camera.getProjectionMatrix((float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);

        base.setMat4("projection", projection);
        base.setMat4("view", view);

        // draw our cube
        base.use();
        obj.draw();
 
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Tab toggles wireframe / fill mode on key-press (edge-detected)
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

    float cameraSpeed = 20.0f * deltaTime;
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

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    static float yaw   = -90.0f; // yaw is initialized to -90.0 degrees
    static float pitch = 0.0f;

    yaw   += xoffset;
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