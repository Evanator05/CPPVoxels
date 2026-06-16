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
        void FreeChunk(uint32_t index);

        uint32_t GetChunkIndex(glm::ivec3 position);
        
        Chunk *GetChunkFromIndex(uint32_t index);
    
        void SetVoxel(ChunkData *chunk, glm::uvec3 position, Voxel voxel);
        Voxel GetVoxel(const ChunkData *chunk, glm::uvec3 position);

        void GenerateChunkOccupancyMap(void);

        size_t GetChunkDataAllocatedBytes(void) const; // returns allocated data byte count


    private:
        std::vector<ChunkData> chunk_data{};
        std::vector<Chunk> allocated_chunks{};
        std::vector<uint32_t> free_spots{};
        ChunkPositions chunk_occupancy{};
};