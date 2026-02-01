#include "voxel.h"

Arena<Voxel> voxelData = Arena<Voxel>(1);
Allocator<Chunk> chunkData;
//Allocator<Model> modelData;

ChunkOccupancyMap chunkOccupancyMapData{};

void voxel_init(void) {
    voxelData.resize(CHUNKWIDTH*CHUNKWIDTH*CHUNKWIDTH*27); // preallocating 3000 chunks of memory
    chunkData.allocateEmpty(1);
    //modelData.allocateEmpty(1);
}

VoxelRegion voxel_dataAllocate(glm::ivec3 size) {
    VoxelRegion vr{};
    vr.size = size;
    vr.index = voxelData.allocate(size.x*size.y*size.z).start;
    return vr;
}

void voxel_dataFree(VoxelRegion data) {
    cArenaSpan a{};
    a.start = data.index;
    a.size = data.size.x*data.size.y*data.size.z;
    voxelData.free(a);
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

// RayResult voxel_raycast_world(Ray ray) {
    
// }

// RayResult voxel_raycast_chunk(Chunk chunk, Ray ray) {
//     RayResult result{};

//     // move ray into chunk space
//     ray.pos -= chunk.pos*CHUNKWIDTH;
//     glm::ivec3 step = glm::sign(ray.dir);

//     ray.pos += glm::vec3(step) * 1e-3f;

//     glm::ivec3 voxelPos = glm::floor(ray.pos);

//     glm::vec3 tMax = ((glm::vec3(voxelPos) + glm::vec3(step) * 0.5f + 0.5f) - ray.pos) / ray.dir;
//     glm::vec3 tDelta = glm::abs(1.0f/ray.dir);

//     while (voxelPos.x >= 0 && voxelPos.x < CHUNKWIDTH && voxelPos.y >= 0 && voxelPos.y < CHUNKWIDTH && voxelPos.z >= 0 && voxelPos.z < CHUNKWIDTH) {
//         uint32_t index = voxelPos.x +
//                          voxelPos.y * CHUNKWIDTH +
//                          voxelPos.z * CHUNKWIDTH * CHUNKWIDTH;
//         Voxel data = voxelData[chunk.data.index + index];

//         float tMin = glm::min(glm::min(tMax.x, tMax.y), tMax.z);
//         glm::ivec3 stepMask = glm::ivec3(
//             tMax.x == tMin,
//             tMax.y == tMin,
//             tMax.z == tMin
//         );

//         if (data.data & VOXELSOLID) {
//             result.hit = true;
//             result.pos = glm::vec3(ray.pos + ray.dir*tMin);
//             result.voxel = data;
//             result.normal = glm::vec3(-stepMask*step);
//             return result;
//         }

//         // step the ray
//         voxelPos += step*stepMask;
//         tMax += glm::vec3(
//             stepMask.x != 0 ? tDelta.x : 0.0,
//             stepMask.y != 0 ? tDelta.y : 0.0,
//             stepMask.z != 0 ? tDelta.z : 0.0
//         );
//     };

//     result.hit = false;
//     return result;
// }

