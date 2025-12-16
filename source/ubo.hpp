#pragma once

#include <glad/glad.h>
#include <vector>
#include <iostream>

class UniformBuffer {
public:
    UniformBuffer(size_t size, GLenum usage = GL_DYNAMIC_DRAW);
    ~UniformBuffer();
    
    // Prevent copying
    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    
    // Allow moving
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;
    
    // Bind to a specific binding point
    void bindToBindingPoint(unsigned int bindingPoint);

    // Bind the UBO
    void bind() const;
    
    // Update entire buffer
    void update(const void* data, size_t size, size_t offset = 0);
    
    // Template version for convenience
    template<typename T>
    void update(const std::vector<T>& data, size_t offset = 0) {
        update(data.data(), data.size() * sizeof(T), offset);
    }
    
    // Template version for single item
    template<typename T>
    void update(const T& data, size_t offset = 0) {
        update(&data, sizeof(T), offset);
    }
    
    // Get UBO handle
    unsigned int getHandle() const { return UBO; }
    size_t getSize() const { return bufferSize; }
    
private:
    unsigned int UBO;
    size_t bufferSize;
    GLenum usagePattern;
};