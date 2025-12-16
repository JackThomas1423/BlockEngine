#include "ubo.hpp"

UniformBuffer::UniformBuffer(size_t size, GLenum usage)
    : UBO(0), bufferSize(size), usagePattern(usage) {
    glGenBuffers(1, &UBO);
    glBindBuffer(GL_UNIFORM_BUFFER, UBO);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, usage);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

UniformBuffer::~UniformBuffer() {
    if (UBO != 0) {
        glDeleteBuffers(1, &UBO);
    }
}

UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept
    : UBO(other.UBO), bufferSize(other.bufferSize), usagePattern(other.usagePattern) {
    other.UBO = 0;
}

UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        if (UBO != 0) {
            glDeleteBuffers(1, &UBO);
        }
        UBO = other.UBO;
        bufferSize = other.bufferSize;
        usagePattern = other.usagePattern;
        other.UBO = 0;
    }
    return *this;
}

void UniformBuffer::bind() const {
    glBindBuffer(GL_UNIFORM_BUFFER, UBO);
}

void UniformBuffer::bindToBindingPoint(unsigned int bindingPoint) {
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, UBO);
}

void UniformBuffer::update(const void* data, size_t size, size_t offset) {
    if (offset + size > bufferSize) {
        std::cout << "ERROR: UBO update exceeds buffer size" << std::endl;
        return;
    }
    
    glBindBuffer(GL_UNIFORM_BUFFER, UBO);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
