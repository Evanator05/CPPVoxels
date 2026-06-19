#include <cstdint>

static constexpr uint8_t CONTREE_NODE_WIDTH = 4;
static constexpr uint8_t CONTREE_MAX_DEPTH = 5;
static constexpr uint16_t CHUNK_WIDTH = 1024; // CONTREE_NODE_WIDTH^CONTREE_MAX_DEPTH
static constexpr uint32_t CHUNK_FLAG_EXISTS = 0b00000000000000000000000000000001;
static constexpr uint32_t CHUNK_FLAG_DIRTY  = 0b00000000000000000000000000000010;
static constexpr uint32_t POINTER_EMPTY = UINT32_MAX;
struct Voxel {
    uint16_t data = 0;

    uint8_t r() const { return data & 0x1F; }
    uint8_t g() const { return (data >> 5) & 0x1F; }
    uint8_t b() const { return (data >> 10) & 0x1F; }
    bool solid() const { return data & 0x8000; }

    void set_r(uint8_t v) { data = (data & ~0x001F) | (v & 0x1F); }
    void set_g(uint8_t v) { data = (data & ~0x03E0) | ((v & 0x1F) << 5); }
    void set_b(uint8_t v) { data = (data & ~0x7C00) | ((v & 0x1F) << 10); }
    void set_rgb(uint8_t r, uint8_t g, uint8_t b) { set_r(r); set_g(g); set_b(b); }

    void set_solid(bool v) {
        if (v) data |= 0x8000;
        else data &= ~0x8000;
    }
};

static constexpr Voxel VOXEL_EMPTY = Voxel{};

struct Chunk {
    glm::ivec3 position{}; // the position in chunk space of this chunk
    uint32_t flags; // flags about the chunk
    uint32_t chunk_data_index; // index to base contree node
};

struct ContreeNode {
    uint64_t isVoxelMask = 0; // bit mask stating if childNode is voxel data or child pointer
    uint32_t childNodes[CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH]; // pointers to child nodes

    uint8_t GetIndex(glm::uvec3 position) {
        return position.x + position.y * CONTREE_NODE_WIDTH + position.z * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH;
    }

    uint32_t GetChildValue(uint8_t index) {
        return childNodes[index];
    }

    bool IsVoxel(uint8_t index) { // if true the node has a voxel data, if false the value is a node
        return (isVoxelMask >> index) & 1ULL;
    }

    void SetValue(uint8_t index, bool isVoxel, uint32_t value) {
        childNodes[index] = value;
        if (isVoxel) isVoxelMask |= 1ULL << index;
        else isVoxelMask &= ~(1ULL<<index);
    }
};

struct ChunkPositions {
    glm::ivec3 position{};
    glm::uvec3 size{};
    uint32_t *chunk_indices = nullptr; // an array of indicies into a chunks array

    uint32_t get_size(void) { return size.x*size.y*size.z; }
};