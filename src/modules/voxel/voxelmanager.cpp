#include "voxelmanager.h"

#include "glm/common.hpp"

void VoxelManager::Init() {

}

void VoxelManager::Process() {

}

void VoxelManager::Shutdown() {
    delete[] chunk_occupancy.chunk_indices;
    chunk_data.reserve(0);
    allocated_chunks.reserve(0);
    free_spots.reserve(0);
}

uint32_t VoxelManager::AllocateChunk(const glm::ivec3 position) {
    uint32_t data_index;

    if (free_spots.empty()) { // if empty allocate more chunk data
        chunk_data.push_back({});
        data_index = static_cast<uint32_t>(chunk_data.size() - 1);
    } else { // otherwise grab existing free data
        data_index = free_spots.back();
        free_spots.pop_back();
    }

    allocated_chunks.push_back({
        position,
        CHUNK_FLAG_EXISTS,
        data_index
    });

    return allocated_chunks.size() - 1;
}

void VoxelManager::FreeChunk(const uint32_t index) {
    Chunk &c = allocated_chunks[index];

    // free chunks data
    free_spots.push_back(c.chunk_data_index);

    // remove chunk
    std::swap(allocated_chunks[index], allocated_chunks.back());
    allocated_chunks.pop_back();
}

uint32_t VoxelManager::GetChunkIndex(const glm::ivec3 position) {
    glm::ivec3 maxBound = chunk_occupancy.position + glm::ivec3(chunk_occupancy.size);
    if (glm::any(glm::lessThan(position, chunk_occupancy.position) || glm::greaterThanEqual(position, maxBound))) {
        return UINT32_MAX;
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

void VoxelManager::SetVoxel(ChunkData *chunk, const glm::uvec3 position, const Voxel voxel) {
    uint32_t index = position.x+position.y*CHUNK_WIDTH+position.z*CHUNK_WIDTH*CHUNK_WIDTH;
    if (index >= CHUNK_WIDTH*CHUNK_WIDTH*CHUNK_WIDTH) return;
    chunk->data[index] = voxel;
}

Voxel VoxelManager::GetVoxel(const ChunkData *chunk, const glm::uvec3 position) {
    uint32_t index = position.x+position.y*CHUNK_WIDTH+position.z*CHUNK_WIDTH*CHUNK_WIDTH;
    if (index >= CHUNK_WIDTH*CHUNK_WIDTH*CHUNK_WIDTH) return VOXEL_EMPTY;
    return chunk->data[index];
}

void VoxelManager::GenerateChunkOccupancyMap() {
    if (allocated_chunks.empty()) {
        free(chunk_occupancy.chunk_indices);
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
        chunk_occupancy.chunk_indices[i] = UINT32_MAX;
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
    return chunk_data.capacity() * sizeof(ChunkData);
}