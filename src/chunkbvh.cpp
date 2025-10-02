#include <vector>
#include <algorithm>
#include <ranges>
#include "math.h"

#include "chunkbvh.h"
#include "glm/vec3.hpp"

#include "stdio.h"

Allocator<BVHNode> chunkBVHData;

void bvh_buildFromChunks(Allocator<Chunk> chunks) {
    std::vector<Chunk*> pchunks;
    for (Chunk &c : chunks.allocationData) {
        pchunks.push_back(&c);
    }
    bvh_buildFromChunks(pchunks);

    // for (BVHNode node : chunkBVHData.allocationData) {
    //     printf("MIN : %u %u %u MAX : %u %u %u CENTER : %u %u %u\n", node.min.x, node.min.y, node.min.z, node.max.x, node.max.y, node.max.z, node.center.x, node.center.y, node.center.z);
    // }
}

uint32_t bvh_buildFromChunks(std::vector<Chunk*>& chunksInNode) {
    if (chunksInNode.size() == 1) { // if the node only has 1 chunk break free
        return 0;    
    } 

    BVHNode node{};

    // find center point min and max
    int centerIndex = chunksInNode.size()/2;
    glm::ivec3 centerPosition;

    std::sort(chunksInNode.begin(), chunksInNode.end(),
    [](const Chunk* a, const Chunk* b) {
        return a->pos.x < b->pos.x;
    });
    centerPosition.x = chunksInNode[centerIndex]->pos.x; // get the center node  with the median x value
    node.min.x = chunksInNode[0]->pos.x; // get the first node with the smallest x value
    node.max.x = chunksInNode[chunksInNode.size()-1]->pos.x+chunksInNode[chunksInNode.size()-1]->data.size.x/64; // get the last node with the biggest x value

    std::sort(chunksInNode.begin(), chunksInNode.end(),
    [](const Chunk* a, const Chunk* b) {
        return a->pos.y < b->pos.y;
    });
    centerPosition.y = chunksInNode[centerIndex]->pos.y;
    node.min.y = chunksInNode[0]->pos.y;
    node.max.y = chunksInNode[chunksInNode.size()-1]->pos.y+chunksInNode[chunksInNode.size()-1]->data.size.y/64;

    std::sort(chunksInNode.begin(), chunksInNode.end(),
    [](const Chunk* a, const Chunk* b) {
        return a->pos.z < b->pos.z;
    });
    centerPosition.z = chunksInNode[centerIndex]->pos.z;
    node.min.z = chunksInNode[0]->pos.z;
    node.max.z = chunksInNode[chunksInNode.size()-1]->pos.z+chunksInNode[chunksInNode.size()-1]->data.size.z/64;

    node.center = centerPosition*64;
    node.max *= 64;
    node.min *= 64;

    // split nodes up by center point
    std::vector<Chunk*> groups[8];
    for(Chunk *c : chunksInNode) {
        uint8_t octant = 
            (c->pos.x >= centerPosition.x) |
            ((c->pos.y >= centerPosition.y) << 1) | 
            ((c->pos.z >= centerPosition.z) << 2);
        groups[octant].push_back(c);
    }

    // allocate bvh node
    Allocation a = chunkBVHData.allocateData(1);

    // recurse
    for (uint8_t i = 0; i < 8; i++) {
        if (groups[i].size() == 1) { // set the node to have a chunk if there is only 1 chunk left in it
            node.hasChunk |= 1<<i;
        }

        if (!groups[i].empty()) {
            node.childIndicies[i] = bvh_buildFromChunks(groups[i]);
        } else {
            node.childIndicies[i] = 0;
        }
    }
    
    chunkBVHData[a.index] = node;
    return a.index;
}