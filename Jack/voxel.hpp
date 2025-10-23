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
typedef std::vector<float> Mesh;

struct Chunk {
    glm::vec3 global_position;
    ColorId data[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
};

Mesh meshChunk(Chunk chunk);

std::vector<float> voxelToMesh();
std::vector<unsigned int> cubeConnector();

Mesh voxelPlain();
std::vector<unsigned int> genConnectors();