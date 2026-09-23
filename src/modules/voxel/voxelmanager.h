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
        void PostProcess(void) override;
        void Shutdown(void) override;

        uint32_t AllocateContreeNode(void);
        void FreeContreeNode(uint32_t root_index);
        uint32_t AllocateChunk(glm::ivec3 position);
        void FreeChunk(uint32_t chunk_index);

        uint32_t GetChunkIndex(glm::ivec3 position);

        glm::ivec3 GetChunkPosition(glm::ivec3 world_position);

        // global space getting and setting voxels
        void SetVoxel(glm::ivec3 position, Voxel voxel);
        Voxel GetVoxel(glm::ivec3 position);
        // chunk space getting and setting voxels
        void SetVoxel(uint32_t chunk_index, glm::uvec3 position, Voxel voxel);
        Voxel GetVoxel(uint32_t chunk_index, glm::uvec3 position);

        void FillNodeUniform(uint32_t node_index, Voxel voxel);

        void FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);
        void FillVoxels(uint32_t node_index, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel);

        uint32_t AllocateSDFNode(Voxel initial_voxel);

        template<uint32_t CellWidth, class SDF>
        bool FillSDFNode(uint32_t& node_index, Voxel initial_voxel, glm::vec3 position, Voxel voxel, SDF& sdf);

        template<class SDF>
        void FillSDFRoot(uint32_t node_index, glm::vec3 position, Voxel voxel, SDF& sdf);

        template<class SDF>
        void FillSDF(Voxel voxel, SDF&& sdf);

        template<class SDF>
        void FillSDF(uint32_t node_index, Voxel voxel, SDF&& sdf);

        template<class SDF>
        void FillSDF(Voxel voxel, SDF&& sdf, glm::ivec3 sdf_min, glm::ivec3 sdf_max);

        void FillSphere(glm::vec3 position, float radius, Voxel voxel);
        
        void GenerateChunkOccupancyMap(void);
        
        void DirtyContreeNode(uint32_t node_index);
        void CleanContreeNodes();

        size_t GetChunkDataAllocatedBytes(void) const; // returns allocated data byte count

        std::vector<ContreeHeader> contree_headers{};
        std::vector<ContreeData> contree_data{};
        std::vector<Chunk> allocated_chunks{};
        ChunkPositions chunk_occupancy{};

        std::vector<uint64_t> dirty_contree_nodes{};
        bool chunks_dirty = false;
    private:
        std::vector<uint32_t> free_contree_indicies{};  
};

#include "voxelmanager_sdf.inl"