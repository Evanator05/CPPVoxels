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

inline int floor_div(int a, int b) {
    return (a >= 0) ? a / b : ((a - (b - 1)) / b);
}

inline int floor_mod(int a, int b) {
    int r = a % b;
    return (r < 0) ? r + b : r;
}

void voxel_delete(glm::ivec3 pos) {
    glm::ivec3 wp = pos;

        

        glm::ivec3 chunkspace = {
            floor_div(wp.x, CHUNKWIDTH),
            floor_div(wp.y, CHUNKWIDTH),
            floor_div(wp.z, CHUNKWIDTH)
        };

        glm::ivec3 cspace = {
            floor_mod(wp.x, CHUNKWIDTH),
            floor_mod(wp.y, CHUNKWIDTH),
            floor_mod(wp.z, CHUNKWIDTH)
        };

        // Bounds check chunk map
        if (chunkspace.x < chunkOccupancyMapData.min.x || chunkspace.x > chunkOccupancyMapData.max.x ||
            chunkspace.y < chunkOccupancyMapData.min.y || chunkspace.y > chunkOccupancyMapData.max.y ||
            chunkspace.z < chunkOccupancyMapData.min.z || chunkspace.z > chunkOccupancyMapData.max.z)
            return;

        glm::ivec3 csize = chunkOccupancyMapData.max - chunkOccupancyMapData.min + glm::ivec3(1);
        glm::ivec3 chunkOffset = chunkspace - chunkOccupancyMapData.min;

        uint32_t chunkMapIndex =
            chunkOffset.x +
            chunkOffset.y * csize.x +
            chunkOffset.z * csize.x * csize.y;

        auto occ = chunkOccupancyMapData.data[chunkMapIndex];

        // Chunk not present
        if (occ.flags == 0)
            return;

        uint32_t voxelIndex =
            chunkData[occ.index].data.index +
            cspace.x +
            cspace.y * CHUNKWIDTH +
            cspace.z * CHUNKWIDTH * CHUNKWIDTH;
        
        Voxel zero{};
        voxelData.set(&zero, voxelIndex);
}

void voxel_delete_sphere(glm::ivec3 center, float radius)
{
    int r = (int)std::ceil(radius);
    float r2 = radius * radius;

    for (int z = -r; z <= r; ++z)
    {
        float z2 = (float)(z * z);
        if (z2 > r2) continue;

        for (int y = -r; y <= r; ++y)
        {
            float yz2 = z2 + (float)(y * y);
            if (yz2 > r2) continue;

            int xMax = (int)std::floor(std::sqrt(r2 - yz2));

            for (int x = -xMax; x <= xMax; ++x)
            {
                voxel_delete(center + glm::ivec3(x, y, z));
            }
        }
    }
}

void voxel_delete_box(glm::ivec3 pos, glm::ivec3 size)
{
    for (int z = 0; z < size.z; ++z)
    for (int y = 0; y < size.y; ++y)
    for (int x = 0; x < size.x; ++x)
    {
        voxel_delete(pos + glm::ivec3(x, y, z));
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

