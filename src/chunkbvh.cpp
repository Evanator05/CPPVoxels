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
}

uint32_t chunkbvh_buildFromChunks(std::vector<Chunk*>& chunksInNode, Chunk* baseChunks) {
    if (chunksInNode.size() == 1) // if the node only has 1 chunk break free
        return 0;    

    BVHNode node{};

    // find min and max
    for (Chunk* c : chunksInNode) {
        node.min.x = std::min(node.min.x, chunksInNode[0]->pos.x);
        node.min.y = std::min(node.min.y, chunksInNode[0]->pos.y);
        node.min.z = std::min(node.min.z, chunksInNode[0]->pos.z);

        node.max.x = std::max(node.max.x, chunksInNode[0]->pos.x+chunksInNode[0]->data.size.x/64);
        node.max.y = std::max(node.max.y, chunksInNode[0]->pos.y+chunksInNode[0]->data.size.y/64);
        node.max.z = std::max(node.max.z, chunksInNode[0]->pos.z+chunksInNode[0]->data.size.z/64);
    }

    // find center points
    int centerIndex = chunksInNode.size()/2;
    auto centerIt = chunksInNode.begin() + chunksInNode.size() / 2;
    glm::ivec3 centerPosition;
    
    std::nth_element(chunksInNode.begin(), centerIt, chunksInNode.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.x < b->pos.x; });
    centerPosition.x = chunksInNode[centerIndex]->pos.x; // get the center node with the median x value

    std::nth_element(chunksInNode.begin(), centerIt, chunksInNode.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.y < b->pos.y; });
    centerPosition.y = chunksInNode[centerIndex]->pos.y; // get the center node with the median y value

    std::nth_element(chunksInNode.begin(), centerIt, chunksInNode.end(),
    [](const Chunk* a, const Chunk* b){ return a->pos.z < b->pos.z; });
    centerPosition.z = chunksInNode[centerIndex]->pos.z; // get the center node with the median z value

    // scale values for worldspace
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