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

        Relptr<ContreeDataBase> AllocateContreeNode(void);
        void FreeContreeNode(Relptr<ContreeDataBase> root);
        //void FreeContreeNode(Relptr<ContreeDataBase> root);
        Relptr<AllocatedChunksBase> AllocateChunk(glm::ivec3 position);
        void FreeChunk(Relptr<AllocatedChunksBase> chunk);

        uint32_t GetChunkIndex(glm::ivec3 position);

        glm::ivec3 GetChunkPosition(glm::ivec3 world_position);

        // global space getting and setting voxels
        void SetVoxel(glm::ivec3 position, Voxel voxel);
        Voxel GetVoxel(glm::ivec3 position);
        // chunk space getting and setting voxels
        void SetVoxel(Relptr<AllocatedChunksBase> chunk, glm::uvec3 position, Voxel voxel);
        Voxel GetVoxel(Relptr<AllocatedChunksBase> chunk, glm::uvec3 position);

        void FillNodeUniform(Relptr<ContreeDataBase> node, Voxel voxel);

        void FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);
        void FillVoxels(Relptr<ContreeDataBase> node, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);

        void FillSDF(Voxel voxel, std::function<float(glm::vec3 pos)>);
        void FillSDF(Relptr<ContreeDataBase> node, Voxel voxel, std::function<float(glm::vec3 pos)>);

        void GenerateChunkOccupancyMap(void);
        
        size_t GetChunkDataAllocatedBytes(void) const; // returns allocated data byte count

        std::string DumpContreeGraph(uint32_t rootIndex);

        std::vector<ContreeNode> contree_data{};
        std::vector<Chunk> allocated_chunks{};
        ChunkPositions chunk_occupancy{};
    private:
        std::vector<uint32_t> free_contree_indicies{};  
};