#include <glad/glad.h>

#include <vector>

class Object {
    public:
        unsigned int VAO, EBO, VBO;

        Object(std::vector<float> vertices, std::vector<unsigned int> indices) {
            glGenBuffers (1, & VBO);
            glGenBuffers (1, & EBO);
            glGenVertexArrays(1, &VAO);

            bindVertexArray();
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW); 

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

        }

        void bindVertexArray() {
            glBindVertexArray(VAO);
        }
};
