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

        uint32_t AllocateContreeNode(void);
        void FreeContreeNode(uint32_t index);

        uint32_t AllocateChunk(glm::ivec3 position);
        void FreeChunk(uint32_t index);

        uint32_t GetChunkIndex(glm::ivec3 position);
        
        Chunk *GetChunkFromIndex(uint32_t index);

        glm::ivec3 GetChunkPosition(glm::ivec3 world_position);

        // global space getting and setting voxels
        void SetVoxel(glm::ivec3 position, Voxel voxel);
        Voxel GetVoxel(glm::ivec3 position);
        // chunk space getting and setting voxels
        void SetVoxel(Chunk *chunk, glm::uvec3 position, Voxel voxel);
        Voxel GetVoxel(const Chunk *chunk, glm::uvec3 position);

        void FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position);

        void GenerateChunkOccupancyMap(void);
        
        size_t GetChunkDataAllocatedBytes(void) const; // returns allocated data byte count


    private:
        std::vector<ContreeNode> contree_data{};
        std::vector<uint32_t> free_contree_indicies{};
        std::vector<Chunk> allocated_chunks{};
        ChunkPositions chunk_occupancy{};
};