#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

// uint8_t is the type used for the colors. this maybe turned into a struct if we need more voxel specific data is needed
// add chunk specific data as needed

// color id of zero should represent air/no-block
typedef uint8_t ColorId;

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

class Chunk {
public:
    glm::vec3 global_position;
    ColorId data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
    Chunk(glm::vec3 position) {
        global_position = position;
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int z = 0; z < CHUNK_DEPTH; ++z) {
                    data[x][y][z] = 0;
                }
            }
        }
    }
};

Mesh meshChunk(Chunk chunk);