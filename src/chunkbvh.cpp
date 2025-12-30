#include <vector>
#include <algorithm>
#include <ranges>
#include "math.h"

#include "chunkbvh.h"
#include "glm/vec3.hpp"

#include "stdio.h"

Allocator<BVHNode> chunkBVHData;

void chunkbvh_buildFromChunks(Allocator<Chunk> chunks) {
    chunkBVHData.clear();

    std::vector<Chunk*> pchunks;
    for (Chunk &c : chunks.allocationData) {
        pchunks.push_back(&c);
    }
    (void)chunkbvh_buildFromChunks(pchunks, chunks.data());

    for (BVHNode n : chunkBVHData.allocationData) {
        printf("MIN %u %u %u MAX %u %u %u CENTER %u %u %u\n", n.min.x, n.min.y, n.min.z, n.max.x, n.max.y, n.max.z, n.center.x, n.center.y, n.center.z);
    }
}

uint32_t chunkbvh_buildFromChunks(std::vector<Chunk*>& chunksInNode, Chunk* baseChunks) {
    if (chunksInNode.size() == 1) // if the node only has 1 chunk break free
        return 0;    

    BVHNode node{};

    // calculate bounding box and center
    chunkbvh_calculateMinMax(chunksInNode, node.min, node.max);
    chunkbvh_calculateCenter(chunksInNode, node.center);

    // scale values for worldspace
    node.center *= 64;
    node.max *= 64;
    node.min *= 64;

    // split nodes up by center point
    std::vector<Chunk*> groups[8];
    for(Chunk *c : chunksInNode) {
        uint8_t octant = 
            (c->pos.x*64 >= node.center.x) |
            ((c->pos.y*64 >= node.center.y) << 1) | 
            ((c->pos.z*64 >= node.center.z) << 2);
        groups[octant].push_back(c);
    }

    // allocate bvh node
    Allocation a = chunkBVHData.allocateData(1);

    // recurse
    for (uint8_t i = 0; i < 8; i++) {
        if (groups[i].size() == 1) { // set the node to have a chunk if there is only 1 chunk left in it
            node.hasChunk |= 1<<i; // flag this group as being a chunk
            node.childIndicies[i] =  groups[i][0] - baseChunks; // get the position of the chunk in the chunk data array
        } else if (!groups[i].empty()) {
            node.childIndicies[i] = chunkbvh_buildFromChunks(groups[i], baseChunks); // get the position of the child bvh node
        } else {
            node.childIndicies[i] = 0;
        }
    }
    
    chunkBVHData[a.index] = node;
    return a.index;
}

void chunkbvh_calculateMinMax(std::vector<Chunk*>& chunks, glm::ivec3 &min, glm::ivec3 &max) {
    min = glm::ivec3(INT_MAX);
    max = glm::ivec3(INT_MIN);

    for (Chunk* c : chunks) {
        min.x = std::min(min.x, c->pos.x);
        min.y = std::min(min.y, c->pos.y);
        min.z = std::min(min.z, c->pos.z);

        max.x = std::max(max.x, c->pos.x+c->data.size.x/64);
        max.y = std::max(max.y, c->pos.y+c->data.size.y/64);
        max.z = std::max(max.z, c->pos.z+c->data.size.z/64);
    }
}

void chunkbvh_calculateCenter(std::vector<Chunk*>& chunks, glm::ivec3 &center) {
    center = glm::ivec3(0);
    // find center points
    int centerIndex = chunks.size()/2;
    auto centerIt = chunks.begin() + chunks.size() / 2;
    
    std::nth_element(chunks.begin(), centerIt, chunks.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.x < b->pos.x; });
    center.x = chunks[centerIndex]->pos.x; // get the center node with the median x value

    std::nth_element(chunks.begin(), centerIt, chunks.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.y < b->pos.y; });
    center.y = chunks[centerIndex]->pos.y; // get the center node with the median y value

    std::nth_element(chunks.begin(), centerIt, chunks.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.z < b->pos.z; });
    center.z = chunks[centerIndex]->pos.z; // get the center node with the median z value
}