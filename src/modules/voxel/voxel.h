#include <cstdint>

static constexpr uint8_t CHUNK_WIDTH = 16;
static constexpr uint32_t CHUNK_FLAG_EXISTS = 0b00000000000000000000000000000001;
static constexpr uint32_t CHUNK_FLAG_DIRTY  = 0b00000000000000000000000000000010;

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
    uint32_t chunk_data_index; // index into a voxel data array
};

struct ChunkData {
    Voxel data[CHUNK_WIDTH*CHUNK_WIDTH*CHUNK_WIDTH];
};

struct ChunkPositions {
    glm::ivec3 position{};
    glm::uvec3 size{};
    uint32_t *chunk_indices = nullptr; // an array of indicies into a chunks array

    uint32_t get_size(void) { return size.x*size.y*size.z; }
};