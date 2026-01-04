#include "voxel.h"
#include <vector>

Allocator<Voxel> voxelData;
Allocator<Chunk> chunkData;
//Allocator<Model> modelData;

ChunkOccupancyMap chunkOccupancyMap;

void voxel_init(void) {
    voxelData.allocateEmpty(CHUNKWIDTH*CHUNKWIDTH*CHUNKWIDTH*27); // preallocating 27 chunks of memory
    chunkData.allocateEmpty(1);
    //modelData.allocateEmpty(1);
}

VoxelRegion voxel_dataAllocate(glm::ivec3 size) {
    VoxelRegion vr{};
    vr.size = size;
    vr.index = voxelData.allocateData(size.x*size.y*size.z).index;
    return vr;
}

void voxel_dataFree(VoxelRegion data) {
    Allocation a{};
    a.index = data.index;
    a.size = data.size.x*data.size.y*data.size.z;
    voxelData.freeData(a);
}

uint32_t voxel_chunkAllocate(glm::ivec3 pos) {
    Chunk c{};
    c.pos = pos;
    Allocation a = chunkData.allocateData(1);
    c.data = voxel_dataAllocate(glm::ivec3(CHUNKWIDTH));
    chunkData[a.index] = c;
    return a.index;
}

void voxel_chunkFree(uint32_t index) {
    chunkData.freeData({index, 1});
}

void voxel_calculateChunkOccupancy() {
    chunkOccupancyMap.min = glm::ivec3(INT_MAX);
    chunkOccupancyMap.max = glm::ivec3(INT_MIN);

    // Find global bounds
    for (const Chunk &c : chunkData.allocationData) {
        chunkOccupancyMap.min = glm::min(chunkOccupancyMap.min, c.pos);
        chunkOccupancyMap.max = glm::max(chunkOccupancyMap.max, c.pos + glm::ivec3(c.data.size) / 64);
    }

    glm::ivec3 gridSize = chunkOccupancyMap.max - chunkOccupancyMap.min;

    // Resize occupancy vector, initialize to -1 (empty)
    chunkOccupancyMap.data.resize(gridSize.x * gridSize.y * gridSize.z, {});

    // Fill vector with chunk indices
    for (uint32_t i = 0; i < chunkData.allocationData.size(); ++i) {
        const Chunk &c = chunkData.allocationData[i];

        uint32_t index = (c.pos.x - chunkOccupancyMap.min.x)
                       + (c.pos.y - chunkOccupancyMap.min.y) * gridSize.x
                       + (c.pos.z - chunkOccupancyMap.min.z) * gridSize.x * gridSize.y;

        chunkOccupancyMap.data[index] = {i, CHUNKOCCUPANCY_OCCUPIED}; // store index into allocationData
    }
}