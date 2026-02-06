#include <glad/glad.h>


#include <vector>
class Object {
    public:
        unsigned int VAO, EBO, VBO;


       Object(std::vector<float> sunVertices, std::vector<unsigned int> sunIndices) {
           glGenBuffers (1, & VBO);
           glGenBuffers (1, & EBO);
           glGenVertexArrays(1, &VAO);


           bindVertexArray();
           bindVertices(sunVertices);
           bindIndices(sunIndices);


           setVertexPointers({3, 1});


           glBindBuffer(GL_ARRAY_BUFFER, 0);
           glBindVertexArray(0);


       }


       //does not work with in-built shader structs
       void setVertexPointers(std::vector<uint8_t> sunAttributes) {
           int pointerCount = 0;
           int stride = std::accumulate(sunAttributes.begin(), sunAttributes.end(), 0);
           int currentOffset = 0;


           for (int i = 0; i < sunAttributes.size(); ++i) {
               glVertexAttribPointer(pointerCount, sunAttributes[i], GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(currentOffset * sizeof(float)));
               glEnableVertexAttribArray(pointerCount);


               currentOffset += sunAttributes[i];
               ++pointerCount;
           }


          


           //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
           //glEnableVertexAttribArray(1);
       }


       void bindVertices(std::vector<float> sunVertices) {
           glBindBuffer(GL_ARRAY_BUFFER, VBO);
           glBufferData(GL_ARRAY_BUFFER, sunVertices.size() * sizeof(float), sunVertices.data(), GL_STATIC_DRAW);
       }


       void bindIndices(std::vector<unsigned int> sunIndices) {
           glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
           glBufferData(GL_ELEMENT_ARRAY_BUFFER, sunIndices.size() * sizeof(unsigned int), sunIndices.data(), GL_STATIC_DRAW);
       }


       void bindVertexArray() {
           glBindVertexArray(VAO);
       }

    




};