#pragma once

#include "glm/vec3.hpp"
#include "SDL3/SDL_gpu.h"
#include "allocator.h"
#include "voxel.h"

typedef struct _BVHNode {
    glm::ivec3 min;
    glm::ivec3 max;
    glm::ivec3 center;
    uint32_t childIndicies[8];
    uint8_t hasChunk;
} BVHNode;

extern Allocator<BVHNode> chunkBVHData;

void chunkbvh_buildFromChunks(Allocator<Chunk> chunks);
uint32_t chunkbvh_buildFromChunks(std::vector<Chunk*>& chunksInNode, Chunk* baseChunks);
void chunkbvh_calculateMinMax(std::vector<Chunk*>& chunks, glm::ivec3 &min, glm::ivec3 &max);
void chunkbvh_calculateCenter(std::vector<Chunk*>& chunks, glm::ivec3 &center);