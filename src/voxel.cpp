#include "voxel.h"

Arena<Voxel> voxelData = Arena<Voxel>(1, CHUNKWIDTH*CHUNKWIDTH*CHUNKWIDTH);
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

inline void voxel_delete(glm::ivec3 pos)
{
    // ---- Cache immutable globals locally (helps optimizer) ----
    const glm::ivec3 minC = chunkOccupancyMapData.min;
    const glm::ivec3 maxC = chunkOccupancyMapData.max;
    const glm::ivec3 csize = maxC - minC + glm::ivec3(1);

    // ---- Fast chunk / local coords ----
    const int wx = pos.x;
    const int wy = pos.y;
    const int wz = pos.z;

    const int cx = floor_div(wx, CHUNKWIDTH);
    const int cy = floor_div(wy, CHUNKWIDTH);
    const int cz = floor_div(wz, CHUNKWIDTH);

    // Bounds check
    if ((unsigned)(cx - minC.x) >= (unsigned)csize.x ||
        (unsigned)(cy - minC.y) >= (unsigned)csize.y ||
        (unsigned)(cz - minC.z) >= (unsigned)csize.z)
        return;

    const int lx = floor_mod(wx, CHUNKWIDTH);
    const int ly = floor_mod(wy, CHUNKWIDTH);
    const int lz = floor_mod(wz, CHUNKWIDTH);

    // ---- Chunk map index ----
    const int ox = cx - minC.x;
    const int oy = cy - minC.y;
    const int oz = cz - minC.z;

    const uint32_t chunkMapIndex =
        ox +
        oy * csize.x +
        oz * csize.x * csize.y;

    const auto& occ = chunkOccupancyMapData.data[chunkMapIndex];
    if (occ.flags == 0)
        return;

    // ---- Final voxel index ----
    const uint32_t voxelIndex =
        chunkData[occ.index].data.index +
        lx +
        ly * CHUNKWIDTH +
        lz * CHUNKWIDTH * CHUNKWIDTH;

    Voxel zero{};
    voxelData.set(&zero, voxelIndex);
}

#include "string.h"
void voxel_delete_sphere(glm::ivec3 center, float radius)
{
    const int r  = (int)std::ceil(radius);
    const float r2 = radius * radius;

    // Chunk bounds
    const int cminx = (center.x - r) >> 6;
    const int cminy = (center.y - r) >> 6;
    const int cminz = (center.z - r) >> 6;

    const int cmaxx = (center.x + r) >> 6;
    const int cmaxy = (center.y + r) >> 6;
    const int cmaxz = (center.z + r) >> 6;

    const glm::ivec3 mapMin = chunkOccupancyMapData.min;
    const glm::ivec3 mapMax = chunkOccupancyMapData.max;
    const glm::ivec3 csize  = mapMax - mapMin + glm::ivec3(1);

    cArena *arena = &voxelData.c_Arena; // alias once

    // ------------------------------------------------------------
    // Iterate chunks
    // ------------------------------------------------------------
    for (int cz = cminz; cz <= cmaxz; ++cz)
    for (int cy = cminy; cy <= cmaxy; ++cy)
    for (int cx = cminx; cx <= cmaxx; ++cx)
    {
        // Chunk bounds check (once)
        const int ox = cx - mapMin.x;
        const int oy = cy - mapMin.y;
        const int oz = cz - mapMin.z;

        if ((unsigned)ox >= (unsigned)csize.x ||
            (unsigned)oy >= (unsigned)csize.y ||
            (unsigned)oz >= (unsigned)csize.z)
            continue;

        const uint32_t chunkMapIndex =
            ox + oy * csize.x + oz * csize.x * csize.y;

        const auto &occ = chunkOccupancyMapData.data[chunkMapIndex];
        if (occ.flags == 0)
            continue;

        const uint32_t baseVoxel =
            chunkData[occ.index].data.index;

        // Chunk world origin
        const int wx0 = cx << 6;
        const int wy0 = cy << 6;
        const int wz0 = cz << 6;

        // Clamp local bounds
        const int lx0 = std::max(0, center.x - r - wx0);
        const int ly0 = std::max(0, center.y - r - wy0);
        const int lz0 = std::max(0, center.z - r - wz0);

        const int lx1 = std::min(CHUNKWIDTH - 1, center.x + r - wx0);
        const int ly1 = std::min(CHUNKWIDTH - 1, center.y + r - wy0);
        const int lz1 = std::min(CHUNKWIDTH - 1, center.z + r - wz0);

        bool anyDirty = false;

        // --------------------------------------------------------
        // Tight voxel loops (bulk clears)
        // --------------------------------------------------------
        for (int lz = lz0; lz <= lz1; ++lz)
        {
            const float dz = float(wz0 + lz - center.z);
            const float dz2 = dz * dz;
            if (dz2 > r2) continue;

            for (int ly = ly0; ly <= ly1; ++ly)
            {
                const float dy = float(wy0 + ly - center.y);
                const float dyz2 = dz2 + dy * dy;
                if (dyz2 > r2) continue;

                const float rem = r2 - dyz2;
                const int dxMax = (int)std::floor(std::sqrt(rem));

                int xs = std::max(lx0, center.x - dxMax - wx0);
                int xe = std::min(lx1, center.x + dxMax - wx0);
                if (xs > xe) continue;

                const uint32_t voxelIndex =
                    baseVoxel +
                    xs +
                    ly * CHUNKWIDTH +
                    lz * CHUNKWIDTH * CHUNKWIDTH;

                const size_t count = xe - xs + 1;

                // Bulk clear voxels
                memset(
                    &arena->data.data[voxelIndex * arena->data.elem_size],
                    0,
                    count * arena->data.elem_size
                );

                // Bulk dirty mark (inline)
                const size_t start = voxelIndex;
                const size_t end   = voxelIndex + count - 1;
                const size_t firstBit = start / arena->dirty_size;
                const size_t lastBit  = end   / arena->dirty_size;

                uint64_t *bits = (uint64_t *)arena->dirty.data;

                size_t fw = firstBit / 64;
                size_t lw = lastBit  / 64;

                uint64_t fm = ~0ULL << (firstBit & 63);
                uint64_t lm = ~0ULL >> (63 - (lastBit & 63));

                if (fw == lw)
                {
                    bits[fw] |= (fm & lm);
                }
                else
                {
                    bits[fw] |= fm;
                    for (size_t w = fw + 1; w < lw; ++w)
                        bits[w] = ~0ULL;
                    bits[lw] |= lm;
                }

                anyDirty = true;
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

