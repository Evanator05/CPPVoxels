#include <stdint.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "allocator.h"

#define VOXELRED      0b0000000000011111
#define VOXELGREEN    0b0000001111100000
#define VOXELBLUE     0b0111110000000000
#define VOXELSOLID    0b1000000000000000

#define CHUNKWIDTH 64

//seeebbbbggggrrrr
typedef struct _Voxel {
    uint16_t data;
} Voxel;

typedef struct _VoxelRegion {
    uint32_t index;
    glm::ivec3 size;
} VoxelRegion;

typedef struct _Chunk {
    glm::ivec3 pos;
    VoxelRegion data;
    uint32_t pad;
} Chunk;

typedef struct _FreeBlock {
    uint32_t index;
    uint32_t size;
} FreeBlock;

typedef struct _AABB {
    glm::ivec3 pos;
    glm::ivec3 size;
} AABB;

typedef struct _Model {
    glm::mat4x4 transform;
    AABB aabb;
    VoxelRegion data;
} Model;

extern Allocator<Voxel> voxelData;
extern Allocator<Chunk> chunkData;
extern Allocator<Model> modelData;

void voxel_init(void);

/**
 * @brief Allocates a block of voxels in the global pool.
 *
 * @param size The 3D dimensions (width, height, depth) of the voxel region
 * @return `Data`: The location and size of the allocation
 * @warning Should not be called directly. Use `voxel_chunkAllocate` to allocate data
 */
VoxelRegion voxel_dataAllocate(glm::ivec3 size);

/**
 * @brief Allocates a chunk of voxels in the global 'voxels' vector.
 *
 * @param pos The 3D position (x, y, z) of the chunk
 * @return The starting index of the allocated chunk header in the `chunk` vector
 */
uint32_t voxel_chunkAllocate(glm::ivec3 pos);

/**
 * @brief Frees a block of voxels and its corrisponding data header from the global pool.
 *
 * @param data The data to free from the global `voxels` vector
 * @warning Should not be called directly. Use `voxel_chunkFree` to free data.
 */
void voxel_dataFree(VoxelRegion data);

/**
 * @brief Frees a chunk and its corrisponding data from the global pool.
 *
 * @param index The index to free from the global `chunks` vector
 */
void voxel_chunkFree(uint32_t index);
