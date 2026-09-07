#pragma once

#include "engine.h"

#include <vector>
#include <functional>

#include "glm/vec3.hpp"

#include "voxel.h"


class VoxelManager : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        uint32_t AllocateContreeNode(void);
        void FreeContreeNode(uint32_t root);
        //void FreeContreeNode(uint32_t root);
        uint32_t AllocateChunk(glm::ivec3 position);
        void FreeChunk(uint32_t chunk);

        uint32_t GetChunkIndex(glm::ivec3 position);

        glm::ivec3 GetChunkPosition(glm::ivec3 world_position);

        // global space getting and setting voxels
        void SetVoxel(glm::ivec3 position, Voxel voxel);
        Voxel GetVoxel(glm::ivec3 position);
        // chunk space getting and setting voxels
        void SetVoxel(uint32_t chunk, glm::uvec3 position, Voxel voxel);
        Voxel GetVoxel(uint32_t chunk, glm::uvec3 position);

        void FillNodeUniform(uint32_t node, Voxel voxel);

        void FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);
        void FillVoxels(uint32_t node, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);

        void FillSDF(Voxel voxel, std::function<float(glm::vec3 pos)>);
        void FillSDF(uint32_t node, Voxel voxel, std::function<float(glm::vec3 pos)>);

        void GenerateChunkOccupancyMap(void);
        
        size_t GetChunkDataAllocatedBytes(void) const; // returns allocated data byte count

        std::vector<ContreeHeader> contree_headers{};
        std::vector<ContreeData> contree_data{};
        std::vector<Chunk> allocated_chunks{};
        ChunkPositions chunk_occupancy{};
    private:
        std::vector<uint32_t> free_contree_indicies{};  
};