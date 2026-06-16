#include "voxelmanager.h"

#include "glm/common.hpp"

void VoxelManager::Init() {

}

void VoxelManager::Process() {

}

void VoxelManager::Shutdown() {
    free(chunk_occupancy.chunk_indices);
}

uint32_t VoxelManager::AllocateChunk(glm::ivec3 position) {

}
uint32_t VoxelManager::FreeChunk(uint32_t index) {

}

uint32_t VoxelManager::GetChunkIndex(glm::ivec3 position) {
    glm::ivec3 maxBound = chunk_occupancy.position + glm::ivec3(chunk_occupancy.size);
    if (glm::any(glm::lessThan(position, chunk_occupancy.position)) ||
        glm::any(glm::greaterThanEqual(position, maxBound))) {
        return UINT32_MAX;
    }

    glm::ivec3 local = position - chunk_occupancy.position;

    size_t index =
        (size_t)local.x +
        (size_t)local.y * chunk_occupancy.size.x +
        (size_t)local.z * chunk_occupancy.size.x * chunk_occupancy.size.y;

    return chunk_occupancy.chunk_indices[index];
}

Chunk* VoxelManager::GetChunkFromIndex(uint32_t index) {
    if (index > allocated_chunks.size()) {
        return nullptr;
    }
    return &allocated_chunks[index];
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

    // Resize occupancy vector
    uint32_t *newMem = (uint32_t*)realloc(chunk_occupancy.chunk_indices, newSize*sizeof(uint32_t));
    
    if (!newMem) return;
    chunk_occupancy.chunk_indices = newMem;
    chunk_occupancy.position = min;
    chunk_occupancy.size = gridSize;

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