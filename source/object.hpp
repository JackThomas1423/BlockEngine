#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <iostream>
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

struct InstanceGroup {
    size_t instanceVBOIndex;
    unsigned int first;
    unsigned int count;
    unsigned int instanceCount;
    
    InstanceGroup(size_t vboIdx, unsigned int f, unsigned int c, unsigned int instCount)
        : instanceVBOIndex(vboIdx), first(f), count(c), instanceCount(instCount) {}
};

struct InstancedAttributeInfo {
    unsigned int vboIndex;      // Index into instanceVBOs
    int attributeLocation;
    int componentCount;
    GLenum type;
    bool normalized;
};

// INFO:
// 
class Object {
    public:
        unsigned int VAO, VBO, EBO;
        std::vector<unsigned int> instanceVBOs;                       // Stores multiple instance buffers
        std::vector<InstanceGroup> instanceGroups;                    // Stores sections of the mesh to instance
        std::vector<InstancedAttributeInfo> instancedAttributeInfos;  // Stores attribute data about each instance group
        GLenum primitiveType;
        size_t vertexCount = 0; // number of vertices (when not using EBO)
        size_t indexCount = 0;  // number of indices (when using EBO)
        
        Object(const void* vertexData, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices = nullptr, GLenum primType = GL_TRIANGLES);
        template <typename T>
        Object(const std::vector<T>& vertexData, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices = nullptr, GLenum primType = GL_TRIANGLES)
            : Object(vertexData.data(), vertexData.size() * sizeof(T), attributes, indices, primType) {}
        
        ~Object();
        
        void bindVertexArray();
        void bindVertices(const void* data, size_t dataSize, GLenum usage = GL_STATIC_DRAW);
        void bindIndices(const std::vector<unsigned int>& indices, GLenum usage = GL_STATIC_DRAW);
        void setVertexPointers(const std::vector<VertexAttribute>& attributes);
        void updateVertices(const void* data, size_t dataSize, size_t offset = 0);

        void addInstanceGroup(size_t instanceVBOIndex, unsigned int first, unsigned int count, unsigned int instanceCount);
        size_t addInstancedAttribute(int attributeLocation, int componentCount, GLenum type, const void* data, size_t dataSize, bool normalized = false, GLenum usage = GL_STATIC_DRAW, int skip = 0);

        void draw();

        template<typename T>
        size_t addInstancedAttribute(int attributeLocation, const std::vector<T>& data, int componentCount, GLenum type, bool normalized = false, GLenum usage = GL_STATIC_DRAW, int skip = 0);
        void updateInstancedAttribute(size_t vboIndex, const void* data, size_t dataSize, size_t offset = 0);
        
        void drawInstanced();
        void drawInstanceGroup(size_t groupIndex);

    private:
        size_t getTypeSize(GLenum type);
        size_t calculateStride(const std::vector<VertexAttribute>& attributes);
};

template<typename T>
size_t Object::addInstancedAttribute(int attributeLocation, const std::vector<T>& data, int componentCount, GLenum type, bool normalized, GLenum usage, int skip)
{
    return addInstancedAttribute(attributeLocation, componentCount, type, data.data(), data.size() * sizeof(T), normalized, usage, skip);
}