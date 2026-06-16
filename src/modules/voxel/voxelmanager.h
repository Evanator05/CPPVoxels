#pragma once

#include "engine.h"

#include <vector>

#include "glm/vec3.hpp"

#include "voxel.h"


class VoxelManager : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        uint32_t AllocateChunk(glm::ivec3 position);
        uint32_t FreeChunk(uint32_t index);

        uint32_t GetChunkIndex(glm::ivec3 position);
        
        Chunk *GetChunkFromIndex(uint32_t index);
    
        void GenerateChunkOccupancyMap(void);

    private:
        struct Allocation {
            uint32_t position;
            uint32_t size;
        };

        ChunkData *chunk_data = NULL;
        std::vector<Chunk> allocated_chunks{};
        std::vector<Allocation> free_allocations{};
        ChunkPositions chunk_occupancy{};

};