#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.hpp"
#include "chunk.hpp"
#include "frustum.hpp"
#include "shader.hpp"
#include "threadPool.hpp"
#include "voxel.hpp"
#include "worldManager.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

// Settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

// Global state
Camera camera(glm::vec3(0.0f, 64.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
              glm::vec3(0.0f, 1.0f, 0.0f), 70.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Forward declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void processInput(GLFWwindow *window);

int main() {
  // Initialize GLFW
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel Engine", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  // Initialize systems
  ThreadPool loadingPool(4);
  ThreadPool updatePool(4);
  WorldManager worldManager(32);

  Shader baseShader("source/base.vs", "source/base.fs");

  // OpenGL state
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glEnable(GL_DEPTH_TEST);

  // Create frustum for culling
  Frustum frustum;

  std::cout << "Window opened!" << std::endl;
  std::cout << "Controls:" << std::endl;
  std::cout << "  WASD - Move horizontally" << std::endl;
  std::cout << "  QE - Move up/down" << std::endl;
  std::cout << "  TAB - Toggle wireframe" << std::endl;
  std::cout << "  ESC - Exit" << std::endl;

  // Start world management thread
  worldManager.start(&loadingPool, &updatePool);

  // Stats tracking
  float lastInfoTime = 0.0f;
  int chunksRendered = 0;
  size_t totalVertices = 0;

  // Reuse this allocation across frames to avoid per-frame heap churn.
  std::vector<Chunk *> visibleChunks;

  // Main render loop
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    // Clear screen
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(
        (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);

    uint8_t visibleQuadFlag = 0;

    for (size_t f = 0; f < 6; ++f) {
      float dot = glm::dot(camera.getFront(), Voxel::FaceNormals[f]);
      if (dot < 0.0f) visibleQuadFlag |= (1 << f);
    }

    // Update frustum (also precomputes pVertexDot for isChunkVisible)
    glm::mat4 projectionView = projection * view;
    frustum.update(projectionView);

    // Update world manager with camera position
    glm::vec3 cameraPos = camera.getPosition();
    worldManager.updateCameraPosition(cameraPos);

    chunksRendered = 0;
    totalVertices = 0;

    baseShader.use();
    baseShader.setMat4("projection", projection);
    baseShader.setMat4("view", view);

    // --- Frustum culling ---
    // We iterate activeChunkWorldPos — a flat array of vec3s with no pointer
    // chasing — and only dereference the Chunk* for chunks that pass the test.
    // markedForDeletion is checked after the frustum test so geometry rejects
    // most chunks before we pay for the atomic load.
    visibleChunks.clear();
    auto startCull = std::chrono::high_resolution_clock::now();
    {
      std::shared_lock<std::shared_mutex> lock(worldManager.getActiveChunksMutex());
      const auto &chunks   = worldManager.getActiveChunks();
      const auto &worldPos = worldManager.getActiveChunkWorldPos();
      const size_t count   = chunks.size();

      for (size_t i = 0; i < count; ++i) {
        // Hot path: only touches the flat float array — no Chunk* dereference.
        if (!frustum.isChunkVisible(worldPos[i]))
          continue;

        // Cold path: chunk passed frustum, now check if it's being deleted.
        Chunk *chunk = chunks[i];
        if (chunk->markedForDeletion.load(std::memory_order_acquire))
          continue;

        visibleChunks.push_back(chunk);
      }
    }
    auto endCull = std::chrono::high_resolution_clock::now();
    float timeCull = std::chrono::duration<float, std::milli>(endCull - startCull).count();

    auto start = std::chrono::high_resolution_clock::now();
    for (Chunk *chunk : visibleChunks) {
      if (!chunk->markedForDeletion.load(std::memory_order_acquire) &&
          chunk->status == ChunkState::WAITING_FOR_MESH_UPDATE) {
          worldManager.queueMeshUpdate(chunk);
          continue;
      }

      size_t vertex_count = chunk->getVertexCount();
      if (vertex_count == 0) continue;

      glm::ivec3 cp = chunk->chunkPosition;
      baseShader.setUInt("instanceData", Voxel::packChunkData(cp.x, cp.y, cp.z));

      chunk->bindAndDraw();
      totalVertices += vertex_count;
      chunksRendered++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::milli>(end - start).count();

    // Print stats every second
    if (currentFrame - lastInfoTime > 1.0f) {
      lastInfoTime = currentFrame;
      float fps = 1.0f / deltaTime;
      std::cout << "FPS: " << fps
                << " | Chunks loaded: " << worldManager.getLoadedChunkCount()
                << " | Rendered: " << chunksRendered
                << " | Vertices: " << totalVertices << std::endl;
      std::cout << "Frame Time spent rendering: " << time * fps << std::endl;
      std::cout << "Frame Time spent culling  : " << timeCull * fps << std::endl;
      std::cout << "Visible Normals: " << (int)visibleQuadFlag << std::endl;
    }

    worldManager.cleanUpDeletedChunks();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  worldManager.stop();
  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // Toggle wireframe mode
  static bool tabWasPressed = false;
  static bool wireframe = false;
  int tabState = glfwGetKey(window, GLFW_KEY_TAB);
  if (tabState == GLFW_PRESS && !tabWasPressed) {
    wireframe = !wireframe;
    if (wireframe)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    tabWasPressed = true;
  }
  if (tabState == GLFW_RELEASE)
    tabWasPressed = false;

  // Camera movement
  float cameraSpeed = 50.0f * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.translate(cameraSpeed * camera.getFront());
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.translate(-cameraSpeed * camera.getFront());
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.translate(
        -glm::normalize(glm::cross(camera.getFront(), camera.getUp())) *
        cameraSpeed);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.translate(
        glm::normalize(glm::cross(camera.getFront(), camera.getUp())) *
        cameraSpeed);
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.translate(-cameraSpeed * camera.getUp());
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.translate(cameraSpeed * camera.getUp());
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  static bool firstMouse = true;
  static float lastX = SCR_WIDTH / 2.0f;
  static float lastY = SCR_HEIGHT / 2.0f;

  if (firstMouse) {
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