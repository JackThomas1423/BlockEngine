#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <numeric>
#include <variant>
#include <memory>

struct VertexAttribute {
    GLenum type;
    uint8_t componentCount;
    bool normalized;

    VertexAttribute(GLenum t, uint8_t count, bool norm = false) : type(t), componentCount(count), normalized(norm) {}
};

class Object {
    public:
        unsigned int VAO, VBO, EBO;
        std::vector<unsigned int> instanceVBOs;  // Store multiple instance buffers
        
        Object(const void* vertexData, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices = nullptr);
        template <typename T>
        Object(const std::vector<T>& vertexData, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices = nullptr)
            : Object(vertexData.data(), vertexData.size() * sizeof(T), attributes, indices) {}
        
        ~Object();
        
        void bindVertexArray();
        void bindVertices(const void* data, size_t dataSize, GLenum usage = GL_STATIC_DRAW);
        void bindIndices(const std::vector<unsigned int>& indices, GLenum usage = GL_STATIC_DRAW);
        void setVertexPointers(const std::vector<VertexAttribute>& attributes);
        void updateVertices(const void* data, size_t dataSize, size_t offset = 0);

        void addInstancedAttribute(int attributeLocation, int componentCount, GLenum type, const void* data, size_t dataSize, bool normalized = false, GLenum usage = GL_STATIC_DRAW);

        template<typename T>
        void addInstancedAttribute(int attributeLocation, const std::vector<T>& data, int componentCount, GLenum type, bool normalized = false, GLenum usage = GL_STATIC_DRAW);
        void updateInstancedAttribute(size_t vboIndex, const void* data, size_t dataSize, size_t offset = 0);

    private:
        size_t getTypeSize(GLenum type);
        size_t calculateStride(const std::vector<VertexAttribute>& attributes);
};

template<typename T>
void Object::addInstancedAttribute(int attributeLocation, const std::vector<T>& data, int componentCount, GLenum type, bool normalized, GLenum usage)
{
    addInstancedAttribute(attributeLocation, componentCount, type, data.data(), data.size() * sizeof(T), normalized, usage);
}