#pragma once
#include "engine.h"

#include "voxelmanager.h"
#include "glm/vec3.hpp"

struct VoxelFileHeader {
    uint8_t magic[4] = {'V', 'O', 'X', 'L'};
    uint32_t chunk_count = 0;
    uint64_t chunk_start_positions[];

    uint64_t get_size() {
        return sizeof(uint8_t) * 4 + sizeof(uint32_t) + sizeof(uint64_t) * chunk_count;
    }
};

struct ContreeNodeData {
    uint64_t isVoxelMask = 0;
    uint32_t node_data[64];

    uint64_t get_size() {
        return sizeof(uint64_t) + sizeof(uint32_t) * 64;
    }
};

struct ChunkData {
    glm::ivec3 position{};
    uint64_t node_count;
    ContreeNodeData nodes[];

    uint64_t get_size() {
        return sizeof(glm::ivec3) + sizeof(uint64_t) + sizeof(ContreeNodeData)*node_count;
    }
};

struct BinaryBlob {
    uint8_t *data;
    uint64_t max_size = 0;
    uint64_t size = 0;
    
    void append_data(uint8_t *_data, uint64_t _size) {
        while(size+_size > max_size) {
            grow();
        }

        memcpy(data+size, _data, _size);
        size += _size;
    }

    void resize(uint64_t _size) {
        max_size = _size;
        if (data == NULL) {
            data = (uint8_t*)malloc(_size);
            return;
        }
        data = (uint8_t*)realloc(data, _size);
    }

    void grow() {
        if (!max_size) max_size = 1024;
        resize(max_size * 1.5);
    };
};

class VoxelSerializer : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void SaveWorld(void);
        void LoadWorld(void);
    private:

        std::vector<uint8_t> SerializeWorld(VoxelManager &vm);
        void DeserializeWorld(std::vector<uint8_t> data);

        std::vector<uint8_t> SerializeChunk(ChunkData *chunk);
        std::vector<uint8_t> SerializeChunk(VoxelManager &vm, uint32_t chunk_index);
        ChunkData* DeserializeChunk(const std::vector<uint8_t>& data);

};