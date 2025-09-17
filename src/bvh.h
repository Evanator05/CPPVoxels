#include "glm/vec3.hpp"

#include "allocator.h"
#include "voxel.h"

typedef struct _BVHNode {
    glm::ivec3 min;
    glm::ivec3 max;
    glm::ivec3 center;
    uint32_t childIndicies[8];
} BVHNode;

extern Allocator<BVHNode> chunkBVHData;

void bvh_buildFromChunks(Allocator<Chunk> chunks);