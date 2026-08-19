#include <cstdint>
#include "relptr/relptr.hpp"

static constexpr uint8_t CONTREE_NODE_WIDTH = 4;
static constexpr uint8_t CONTREE_MAX_DEPTH = 3;
static constexpr uint64_t CONTREE_VOXEL_MASK_FULL = UINT64_MAX;
static constexpr uint16_t CHUNK_WIDTH = 64; // CONTREE_NODE_WIDTH^CONTREE_MAX_DEPTH
static constexpr uint32_t CHUNK_FLAG_EXISTS = 0b00000000000000000000000000000001;
static constexpr uint32_t CHUNK_FLAG_DIRTY  = 0b00000000000000000000000000000010;
static constexpr uint32_t POINTER_EMPTY = UINT32_MAX;
struct Voxel {
    uint32_t data = 0;

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

    std::string to_string() {
        return std::to_string(r()) + "R " + std::to_string(g()) + "G " + std::to_string(b()) + "B";
    }

    auto operator<=>(const Voxel&) const = default;
    bool operator==(const Voxel&) const = default;
};

static constexpr Voxel VOXEL_EMPTY = Voxel{};

struct ContreeNode;
using ContreeDataBase = RelptrBaseVector<RELPTR_TAG(cb), ContreeNode>;
struct ContreeNode {
    uint64_t isVoxelMask = CONTREE_VOXEL_MASK_FULL; // bit mask stating if child node data is voxel or pointer, defaults to voxel
    union {
        Voxel voxel_data[CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH]{};
        Relptr<ContreeDataBase> child_nodes[CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH];
    };

    size_t GetIndex(glm::uvec3 position) {
        return position.x + position.y * CONTREE_NODE_WIDTH + position.z * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH;
    }

    Relptr<ContreeDataBase> GetPtr(size_t index) {
        return child_nodes[index];
    }

    Voxel GetVoxel(size_t index) {
        return voxel_data[index];
    }

    bool IsVoxel(size_t index) { // if true the node has a voxel data, if false the value is a node
        return (isVoxelMask >> index) & 1ULL;
        
    }

    void SetVoxel(size_t index, Voxel value) {
        voxel_data[index] = value;
        uint64_t bit = 1ULL << index;
        isVoxelMask |= bit;
    }

    void SetPtr(size_t index, Relptr<ContreeDataBase> value) {
        child_nodes[index] = value;
        uint64_t bit = 1ULL << index;
        isVoxelMask &= ~bit;
    }

    bool IsUniform() {
        if (!IsVoxel(0)) return false;

        Voxel value = voxel_data[0];
        
        for (size_t i = 1; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
            if (!IsVoxel(i)) return false;
            if (GetVoxel(i) != value) return false;
        }
        return true;
    }
};

struct Chunk {
    glm::ivec3 position{}; // the position in chunk space of this chunk
    //alignas(16) uint32_t flags = 0; // flags about the chunk
    Relptr<ContreeDataBase> contree_node{};
};

using AllocatedChunksBase = RelptrBaseVector<RELPTR_TAG(ac), Chunk>;

struct ChunkPositionsHeader {
    alignas(16) glm::ivec3 position{};
    alignas(16) glm::uvec3 size{};
};

struct ChunkPositions {
    alignas(16) glm::ivec3 position{};
    alignas(16) glm::uvec3 size{};
    Relptr<AllocatedChunksBase> *chunks = nullptr; // an array of indicies into a chunks array

    uint32_t get_size(void) { return size.x*size.y*size.z; }
};