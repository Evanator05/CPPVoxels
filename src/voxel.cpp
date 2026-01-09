#include "voxel.h"
#include <vector>

Allocator<Voxel> voxelData;
Allocator<Chunk> chunkData;
//Allocator<Model> modelData;

ChunkOccupancyMap chunkOccupancyMapData{};

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
    // Chunk-space bounds
    chunkOccupancyMapData.min = glm::ivec3(INT_MAX);
    chunkOccupancyMapData.max = glm::ivec3(INT_MIN);

    // Find global chunk bounds
    for (const Chunk& c : chunkData.allocationData) {
        chunkOccupancyMapData.min = glm::min(chunkOccupancyMapData.min, c.pos);
        chunkOccupancyMapData.max = glm::max(chunkOccupancyMapData.max, c.pos);
    }

    // Inclusive bounds → +1
    glm::ivec3 gridSize =
        (chunkOccupancyMapData.max - chunkOccupancyMapData.min) + glm::ivec3(1);

    // Resize occupancy vector
    chunkOccupancyMapData.data.clear();
    chunkOccupancyMapData.data.resize(
        gridSize.x * gridSize.y * gridSize.z,
        {}
    );

    // Fill occupancy map
    for (uint32_t i = 0; i < chunkData.allocationData.size(); ++i) {
        const Chunk& c = chunkData.allocationData[i];

        glm::ivec3 local =
            c.pos - chunkOccupancyMapData.min;

        uint32_t index =
            local.x +
            local.y * gridSize.x +
            local.z * gridSize.x * gridSize.y;

        chunkOccupancyMapData.data[index] =
            { i, CHUNKOCCUPANCY_OCCUPIED };
    }
}

RayResult voxel_raycast_world(Ray ray);