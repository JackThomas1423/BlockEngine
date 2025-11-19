#include "object.hpp"

Object::Object(const void* vertexData, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices)
    : VAO(0), VBO(0), EBO(0) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    bindVertexArray();
    bindVertices(vertexData, vertexDataSize);

    if (indices && !indices->empty()) {
        glGenBuffers(1, &EBO);
        bindIndices(*indices);
    }

    setVertexPointers(attributes);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Object::~Object()
{
    for (unsigned int vbo : instanceVBOs) {
        glDeleteBuffers(1, &vbo);
    }
    glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
}

void Object::bindVertexArray() { glBindVertexArray(VAO); }

void Object::bindVertices(const void* data, size_t dataSize, GLenum usage)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, dataSize, data, usage);
}

void Object::updateVertices(const void* data, size_t dataSize, size_t offset)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, offset, dataSize, data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Object::bindIndices(const std::vector<unsigned int>& indices, GLenum usage)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), usage);
}

void Object::addInstancedAttribute(int attributeLocation, int componentCount, GLenum type, const void* data, size_t dataSize, bool normalized, GLenum usage)
{
    bindVertexArray();
    
    unsigned int instanceVBO;
    glGenBuffers(1, &instanceVBO);
    instanceVBOs.push_back(instanceVBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, dataSize, data, usage);
    
    glEnableVertexAttribArray(attributeLocation);
    
    // Use glVertexAttribIPointer for integer types that shouldn't be normalized
    if ((type == GL_INT || type == GL_UNSIGNED_INT ||
         type == GL_BYTE || type == GL_UNSIGNED_BYTE ||
         type == GL_SHORT || type == GL_UNSIGNED_SHORT) && !normalized) {
        glVertexAttribIPointer(attributeLocation, componentCount, type,
                              componentCount * getTypeSize(type), (void*)0);
    } else {
        glVertexAttribPointer(attributeLocation, componentCount, type,
                             normalized ? GL_TRUE : GL_FALSE,
                             componentCount * getTypeSize(type), (void*)0);
    }
    
    glVertexAttribDivisor(attributeLocation, 1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Object::updateInstancedAttribute(size_t vboIndex, const void* data, size_t dataSize, size_t offset)
{
    if (vboIndex < instanceVBOs.size()) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs[vboIndex]);
        glBufferSubData(GL_ARRAY_BUFFER, offset, dataSize, data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

size_t Object::calculateStride(const std::vector<VertexAttribute>& attributes)
{
    size_t stride = 0;
    for (const auto& attr : attributes) {
        stride += attr.componentCount * getTypeSize(attr.type);
    }
    return stride;
}

void Object::setVertexPointers(const std::vector<VertexAttribute>& attributes)
{
    size_t stride = calculateStride(attributes);
    size_t currentOffset = 0;
    
    for (size_t i = 0; i < attributes.size(); ++i) {
        const auto& attr = attributes[i];
        
        // For integer types (not normalized), use glVertexAttribIPointer
        if ((attr.type == GL_INT || attr.type == GL_UNSIGNED_INT ||
             attr.type == GL_BYTE || attr.type == GL_UNSIGNED_BYTE ||
             attr.type == GL_SHORT || attr.type == GL_UNSIGNED_SHORT) && !attr.normalized) {
            glVertexAttribIPointer(i, attr.componentCount, attr.type, stride, (void*)currentOffset);
        } else {
            glVertexAttribPointer(i, attr.componentCount, attr.type, attr.normalized ? GL_TRUE : GL_FALSE, stride, (void*)currentOffset);
        }
        
        glEnableVertexAttribArray(i);
        currentOffset += attr.componentCount * getTypeSize(attr.type);
    }
}

size_t Object::getTypeSize(GLenum type)
{
    switch(type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_HALF_FLOAT:
            return 2;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            return 4;
        case GL_DOUBLE:
            return 8;
        default:
            return 4; // Default to float size
    }
}