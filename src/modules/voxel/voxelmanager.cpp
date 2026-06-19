#include "voxelmanager.h"

#include "glm/common.hpp"

void VoxelManager::Init() {

}

void VoxelManager::Process() {

}

void VoxelManager::Shutdown() {
    delete[] chunk_occupancy.chunk_indices;
    contree_data.reserve(0);
    free_contree_indicies.reserve(0);
    allocated_chunks.reserve(0);
}

uint32_t VoxelManager::AllocateContreeNode(void) {
    uint32_t data_index;
    if (free_contree_indicies.empty()) {
        contree_data.push_back({});
        data_index = static_cast<uint32_t>(contree_data.size() - 1);
    } else {
        data_index = free_contree_indicies.back();
        free_contree_indicies.pop_back();
    }
    contree_data[data_index] = {};
    return data_index;
}

void VoxelManager::FreeContreeNode(uint32_t index) {
    ContreeNode &node = contree_data[index];
    if (node.isVoxelMask != (uint64_t)(-1ULL)) { // if there is any branches in this node otherwise skip searching for child nodes
        for (size_t i = 0; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
            bool isVoxel = node.IsVoxel(i);
            if (isVoxel) continue;
            FreeContreeNode(node.GetChildValue(i));
        }
    }
    node.isVoxelMask = (uint64_t)(-1);
    free_contree_indicies.push_back(index);
}


uint32_t VoxelManager::AllocateChunk(const glm::ivec3 position) {
    allocated_chunks.push_back({
        position,
        CHUNK_FLAG_EXISTS,
        AllocateContreeNode()
    });
    return allocated_chunks.size() - 1;
}

void VoxelManager::FreeChunk(const uint32_t index) {
    Chunk *chunk = GetChunkFromIndex(index);
    FreeContreeNode(chunk->chunk_data_index);

    allocated_chunks[index] = allocated_chunks.back();
    allocated_chunks.pop_back();
}

uint32_t VoxelManager::GetChunkIndex(const glm::ivec3 position) {
    glm::ivec3 maxBound = chunk_occupancy.position + glm::ivec3(chunk_occupancy.size);
    if (glm::any(glm::lessThan(position, chunk_occupancy.position) || glm::greaterThanEqual(position, maxBound))) {
        return POINTER_EMPTY;
    }

    glm::ivec3 local = position - chunk_occupancy.position;

    size_t index =
        (size_t)local.x +
        (size_t)local.y * chunk_occupancy.size.x +
        (size_t)local.z * chunk_occupancy.size.x * chunk_occupancy.size.y;

    return chunk_occupancy.chunk_indices[index];
}

Chunk* VoxelManager::GetChunkFromIndex(const uint32_t index) {
    if (index >= allocated_chunks.size()) {
        return nullptr;
    }
    return &allocated_chunks[index];
}

glm::ivec3 VoxelManager::GetChunkPosition(glm::ivec3 world_position) {
    return glm::ivec3(
        world_position.x >= 0 ? world_position.x / CHUNK_WIDTH : (world_position.x - CHUNK_WIDTH + 1) / CHUNK_WIDTH,
        world_position.y >= 0 ? world_position.y / CHUNK_WIDTH : (world_position.y - CHUNK_WIDTH + 1) / CHUNK_WIDTH,
        world_position.z >= 0 ? world_position.z / CHUNK_WIDTH : (world_position.z - CHUNK_WIDTH + 1) / CHUNK_WIDTH
    );
}

// global space getting and setting voxels
void VoxelManager::SetVoxel(glm::ivec3 position, Voxel voxel) {
    glm::ivec3 chunk_position = GetChunkPosition(position);
    glm::ivec3 local_position = position - chunk_position * glm::ivec3(CHUNK_WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    SetVoxel(GetChunkFromIndex(chunk_index), local_position, voxel);
}
Voxel VoxelManager::GetVoxel(glm::ivec3 position) {
    glm::ivec3 chunk_position = GetChunkPosition(position);
    glm::ivec3 local_position = position - chunk_position * glm::ivec3(CHUNK_WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    return GetVoxel(GetChunkFromIndex(chunk_index), local_position);
}

// chunk space getting and setting voxels
void VoxelManager::SetVoxel(Chunk *chunk, glm::uvec3 position, Voxel voxel) {
    ContreeNode *node = &contree_data[chunk->chunk_data_index];

    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);

    for (uint8_t depth = 0; depth < CONTREE_MAX_DEPTH - 1; depth++) {
        chunk_width /= 4;

        glm::uvec3 nodePos = (position / chunk_width);
        position -= nodePos * chunk_width;

        uint8_t nodeIndex = node->GetIndex(nodePos);
        uint32_t node_value = node->GetChildValue(nodeIndex);
        
        if (node->IsVoxel(nodeIndex)) {
            if (node_value == voxel.data) return; // if the spot is already a voxel and that voxels value is the same as what we are trying to insert do not split

            uint32_t newNodeIndex = AllocateContreeNode();
            
            node->SetValue(nodeIndex, false, newNodeIndex);
            
            // fill new node with data from parent
            ContreeNode *newNode = &contree_data[newNodeIndex];
            for (uint8_t i = 0; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
                newNode->SetValue(i, true, node_value);
            }
            node_value = newNodeIndex;
        }

        node = &contree_data[node_value];
    }

    uint8_t leafIndex = node->GetIndex(position);
    node->SetValue(leafIndex, true, voxel.data);
}

Voxel VoxelManager::GetVoxel(const Chunk *chunk, glm::uvec3 position) {
    ContreeNode *node = &contree_data[chunk->chunk_data_index];
    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);
    
    for (uint8_t i = 0; i < CONTREE_MAX_DEPTH; i++) {
        chunk_width /= 4;
        glm::uvec3 nodePos = (position / chunk_width);
        position -= nodePos * chunk_width;

        uint8_t nodeIndex = node->GetIndex(nodePos);
        if (node->IsVoxel(nodeIndex))
            return (Voxel)node->GetChildValue(nodeIndex);
        node = &contree_data[node->GetChildValue(nodeIndex)];
    }
    return VOXEL_EMPTY;
}

void FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position) {

}

void VoxelManager::GenerateChunkOccupancyMap() {
    if (allocated_chunks.empty()) {
        delete[] chunk_occupancy.chunk_indices;
        chunk_occupancy.chunk_indices = nullptr;
        return;
    }
    // Chunk-space bounds
    glm::ivec3 min = glm::ivec3(INT_MAX);
    glm::ivec3 max = glm::ivec3(INT_MIN);

    // Find global chunk bounds
    for (const Chunk& c : allocated_chunks) {
        min = glm::min(min, c.position);
        max = glm::max(max, c.position);
    }

    glm::ivec3 gridSize = (max - min) + glm::ivec3(1);
    
    size_t newSize = gridSize.x * gridSize.y * gridSize.z; 
    size_t oldSize = chunk_occupancy.get_size();

    // Resize occupancy vector if the size is different
    if (newSize != oldSize) {
        uint32_t *newMem = new uint32_t[newSize];
        if (!newMem) return;
        delete[] chunk_occupancy.chunk_indices;
        chunk_occupancy.chunk_indices = newMem;
        chunk_occupancy.size = gridSize;
    }
    
    chunk_occupancy.position = min;
    
    // Fill map with empty entries
    for (size_t i = 0; i < newSize; ++i) {
        chunk_occupancy.chunk_indices[i] = POINTER_EMPTY;
    }

    // Fill occupancy map
    for (uint32_t i = 0; i < allocated_chunks.size(); ++i) {
        const Chunk& c = allocated_chunks[i];

        glm::ivec3 local = c.position - min;

        uint32_t index =
            local.x +
            local.y * gridSize.x +
            local.z * gridSize.x * gridSize.y;

        chunk_occupancy.chunk_indices[index] = i;
    }
    
}

size_t VoxelManager::GetChunkDataAllocatedBytes() const {
    return contree_data.capacity() * sizeof(ContreeNode);
}