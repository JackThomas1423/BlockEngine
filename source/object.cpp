#include "object.hpp"

Object::Object(const void* vertexData, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes, const std::vector<unsigned int>* indices, GLenum primType)
    : VAO(0), VBO(0), EBO(0), primitiveType(primType) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    bindVertexArray();
    bindVertices(vertexData, vertexDataSize);

    if (indices && !indices->empty()) {
        glGenBuffers(1, &EBO);
        bindIndices(*indices);
    }

    setVertexPointers(attributes);
    // Calculate and store vertex count based on provided vertex data and attribute stride
    size_t stride = calculateStride(attributes);
    if (stride > 0) {
        vertexCount = vertexDataSize / stride;
    } else {
        vertexCount = 0;
    }
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

size_t Object::addInstancedAttribute(int attributeLocation, int componentCount, GLenum type, const void* data, size_t dataSize, bool normalized, GLenum usage, int skip)
{
    bindVertexArray();
    
    unsigned int instanceVBO;
    glGenBuffers(1, &instanceVBO);
    size_t vboIndex = instanceVBOs.size();
    instanceVBOs.push_back(instanceVBO);
    
    // Store metadata
    instancedAttributeInfos.push_back({
        static_cast<unsigned int>(vboIndex),
        attributeLocation,
        componentCount,
        type,
        normalized
    });
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, dataSize, data, usage);
    
    glEnableVertexAttribArray(attributeLocation);
    
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
    
    glVertexAttribDivisor(attributeLocation, skip + 1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    return vboIndex;
}

void Object::addInstanceGroup(size_t instanceVBOIndex, unsigned int first, unsigned int count, unsigned int instanceCount)
{
    if (instanceVBOIndex >= instanceVBOs.size()) {
        std::cout << "ERROR: addition of instance group references out of bounds VBO instances" << std::endl;
        return;
    }
    instanceGroups.emplace_back(instanceVBOIndex, first, count, instanceCount);
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
            std::cout << "OBJECT::WARNNING: unknown type ' " << type << " ' tried to load. defaulting to float (4 BYTES)"<< std::endl;
            return 4; // Default to float size
    }
}

void Object::draw()
{
    bindVertexArray();
    
    if (EBO != 0) {
        glDrawElements(primitiveType, 0, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(primitiveType, 0, static_cast<GLsizei>(vertexCount));
    }
    
    glBindVertexArray(0);
}

void Object::drawInstanced()
{
    bindVertexArray();
    
    for (const auto& group : instanceGroups) {
        drawInstanceGroup(&group - &instanceGroups[0]); // Get index
    }
    
    glBindVertexArray(0);
}

void Object::drawInstanceGroup(size_t groupIndex)
{
    if (groupIndex >= instanceGroups.size()) return;
    
    const auto& group = instanceGroups[groupIndex];
    bindVertexArray();
    
    // Rebind and reconfigure instanced attributes
    for (const auto& attrInfo : instancedAttributeInfos) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs[group.instanceVBOIndex]);
        
        if ((attrInfo.type == GL_INT || attrInfo.type == GL_UNSIGNED_INT ||
             attrInfo.type == GL_BYTE || attrInfo.type == GL_UNSIGNED_BYTE ||
             attrInfo.type == GL_SHORT || attrInfo.type == GL_UNSIGNED_SHORT) 
             && !attrInfo.normalized) {
            glVertexAttribIPointer(attrInfo.attributeLocation, attrInfo.componentCount,
                                  attrInfo.type, attrInfo.componentCount * getTypeSize(attrInfo.type),
                                  (void*)0);
        } else {
            glVertexAttribPointer(attrInfo.attributeLocation, attrInfo.componentCount,
                                 attrInfo.type, attrInfo.normalized ? GL_TRUE : GL_FALSE,
                                 attrInfo.componentCount * getTypeSize(attrInfo.type),
                                 (void*)0);
        }
    }
    
    // Use the stored primitive type
    if (EBO != 0) {
        glDrawElementsInstanced(primitiveType, group.count, GL_UNSIGNED_INT,
                               (void*)(group.first * sizeof(unsigned int)),
                               group.instanceCount);
    } else {
        glDrawArraysInstanced(primitiveType, group.first, group.count,
                             group.instanceCount);
    }
}