#include <glad/glad.h>

#include <vector>
#include <numeric>

class Object {
    public:
        unsigned int VAO, EBO, VBO;

        Object(std::vector<float> vertices, std::vector<unsigned int> indices) {
            glGenBuffers (1, & VBO);
            glGenBuffers (1, & EBO);
            glGenVertexArrays(1, &VAO);

            bindVertexArray();
            bindVertices(vertices);
            bindIndices(indices);

            setVertexPointers({3, 3});

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

        }

        void setVertexPointers(std::vector<uint8_t> attributes) {
            int pointerCount = 0;
            int stride = std::accumulate(attributes.begin(), attributes.end(), 0);
            int currentOffset = 0;

            for (int i = 0; i < attributes.size(); ++i) {
                glVertexAttribPointer(pointerCount, attributes[i], GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(currentOffset * sizeof(float)));
                glEnableVertexAttribArray(pointerCount);

                currentOffset += attributes[i];
                ++pointerCount;
            }

            

            //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            //glEnableVertexAttribArray(1);
        }

        void bindVertices(std::vector<float> vertices) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        }

        void bindIndices(std::vector<unsigned int> indices) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        }

        void bindVertexArray() {
            glBindVertexArray(VAO);
        }
};
