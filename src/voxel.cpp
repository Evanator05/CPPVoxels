#include "voxel.h"
#include <vector>

Allocator<Voxel> voxelData;
Allocator<Chunk> chunkData;
Allocator<Model> modelData;

void voxel_init(void) {
    voxelData.allocateEmpty(CHUNKWIDTH*CHUNKWIDTH*CHUNKWIDTH*27); // preallocating 27 chunks of memory
    chunkData.allocateEmpty(1);
    modelData.allocateEmpty(1);
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