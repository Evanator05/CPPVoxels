#include "voxelmanager.h"

#include "glm/common.hpp"
#include <glm/glm.hpp>
#include <unordered_set>
#include <sstream>

#include "fixedstack/fixedstack.hpp"


void VoxelManager::Init() {
    contree_headers.reserve(10);
    contree_data.reserve(10);
    free_contree_indicies.reserve(10);
    allocated_chunks.reserve(10);
}

void VoxelManager::Process() {

}

void VoxelManager::PostProcess() {
    CleanContreeNodes();
}

void VoxelManager::Shutdown() {
    delete[] chunk_occupancy.chunks;
    contree_data.reserve(0);
    free_contree_indicies.reserve(0);
    allocated_chunks.reserve(0);
}

uint32_t VoxelManager::AllocateContreeNode() {
    uint32_t data_index;
    if (free_contree_indicies.empty()) {
        contree_headers.push_back({});
        contree_data.push_back({});
        data_index = static_cast<uint32_t>(contree_headers.size() - 1);
    } else {
        data_index = free_contree_indicies.back();
        free_contree_indicies.pop_back();
    }
    contree_headers[data_index] = {};
    contree_headers[data_index].isVoxelMask = CONTREE_VOXEL_MASK_FULL;
    contree_headers[data_index].isSolidMask = 0;
    contree_data[data_index] = {};
    
    dirty_contree_nodes.resize(contree_data.size()/64+1);

    DirtyContreeNode(data_index);

    return data_index;
}

void VoxelManager::FreeContreeNode(uint32_t root) {
    if (root == POINTER_EMPTY) return;

    if (contree_headers[root].isVoxelMask != CONTREE_VOXEL_MASK_FULL) {
        for (size_t i = 0; i < ContreeNode::WIDTH * ContreeNode::WIDTH * ContreeNode::WIDTH; i++) {
            if (contree_headers[root].IsVoxel(i)) continue;
            FreeContreeNode(contree_data[root].GetPtr(i));
        }
    }
    contree_headers[root].isVoxelMask = CONTREE_VOXEL_MASK_FULL;
    contree_headers[root].isSolidMask = 0;
    free_contree_indicies.push_back(root);
}

uint32_t VoxelManager::AllocateChunk(const glm::ivec3 position) {
    allocated_chunks.push_back({
        position,
        //CHUNK_FLAG_EXISTS,
        AllocateContreeNode()
    });
    return allocated_chunks.size() - 1;
}

void VoxelManager::FreeChunk(uint32_t chunk) {
    FreeContreeNode(allocated_chunks[chunk].contree_node);
    allocated_chunks[chunk] = allocated_chunks.back();
    allocated_chunks.pop_back();
}

uint32_t VoxelManager::GetChunkIndex(const glm::ivec3 position) {
    glm::ivec3 maxBound = chunk_occupancy.position + glm::ivec3(chunk_occupancy.size);
    if (glm::any(glm::lessThan(position, chunk_occupancy.position) || glm::greaterThanEqual(position, maxBound))) {
        return POINTER_EMPTY;
    }

    glm::ivec3 local = position - chunk_occupancy.position;

    size_t index =
        (size_t)local.x +
        (size_t)local.y * chunk_occupancy.size.x +
        (size_t)local.z * chunk_occupancy.size.x * chunk_occupancy.size.y;

    return chunk_occupancy.chunks[index];
}

glm::ivec3 VoxelManager::GetChunkPosition(glm::ivec3 world_position) {
    return glm::ivec3(
        world_position.x >= 0 ? world_position.x / Chunk::WIDTH : (world_position.x - Chunk::WIDTH + 1) / Chunk::WIDTH,
        world_position.y >= 0 ? world_position.y / Chunk::WIDTH : (world_position.y - Chunk::WIDTH + 1) / Chunk::WIDTH,
        world_position.z >= 0 ? world_position.z / Chunk::WIDTH : (world_position.z - Chunk::WIDTH + 1) / Chunk::WIDTH
    );
}

// global space getting and setting voxels
void VoxelManager::SetVoxel(glm::ivec3 world_position, Voxel voxel) {
    glm::ivec3 chunk_position = GetChunkPosition(world_position);
    glm::ivec3 local_position = world_position - chunk_position * glm::ivec3(Chunk::WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    if (chunk_index == POINTER_EMPTY) return;
    SetVoxel(chunk_index, local_position, voxel);
}

Voxel VoxelManager::GetVoxel(glm::ivec3 world_position) {
    glm::ivec3 chunk_position = GetChunkPosition(world_position);
    glm::ivec3 local_position = world_position - chunk_position * glm::ivec3(Chunk::WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    if (chunk_index == POINTER_EMPTY) return VOXEL_EMPTY;
    return GetVoxel(chunk_index, local_position);
}

void VoxelManager::SetVoxel(uint32_t chunk, glm::uvec3 position, Voxel voxel) {
    struct NodeStack {
        uint32_t node_index;
        uint8_t child_index;
    };
    FixedStack<NodeStack, ContreeNode::MAX_DEPTH> stack;
    
    uint32_t node_index = allocated_chunks[chunk].contree_node;

    glm::uvec3 chunk_width = glm::uvec3(Chunk::WIDTH);

    stack.push({node_index, 0});

    for (uint8_t depth = 0; depth < ContreeNode::MAX_DEPTH - 1; depth++) { // depth - 1 because we dont need to allocate/check on the last layer we just want to set a voxel in it
        chunk_width /= ContreeNode::WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = ContreeNode::GetIndex(node_position);

        if (contree_headers[node_index].IsVoxel(child_node_index)) {
            Voxel child_node_voxel = contree_data[node_index].GetVoxel(child_node_index);
            if (child_node_voxel == voxel) return;

            uint32_t new_node_index = AllocateContreeNode();
            ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
            node.SetPtr(child_node_index, new_node_index);
            DirtyContreeNode(node_index);

            ContreeNode new_node{&contree_headers[new_node_index], &contree_data[new_node_index]};
            for (uint8_t i = 0; i < ContreeNode::WIDTH*ContreeNode::WIDTH*ContreeNode::WIDTH; i++) {
                new_node.SetVoxel(i, child_node_voxel); // fill new node with voxel data from parent
            }

            DirtyContreeNode(new_node_index);
        }

        stack.push({node_index, child_node_index});
        node_index = contree_data[node_index].GetPtr(child_node_index);
    }

    uint8_t child_node_index = ContreeNode::GetIndex(position);

    ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
    node.SetVoxel(child_node_index, voxel);
    DirtyContreeNode(node_index);

    ContreeNode current_node = {&contree_headers[node_index], &contree_data[node_index]};

    while (stack.size() > 0) {
        if (!current_node.IsUniform())
            break;

        Voxel voxel_value = current_node.GetVoxel(0);

        NodeStack parent_info = stack.pop();

        ContreeNode parent_node = {&contree_headers[parent_info.node_index], &contree_data[parent_info.node_index]};

        FreeContreeNode(parent_node.GetPtr(parent_info.child_index));
        parent_node = {&contree_headers[parent_info.node_index], &contree_data[parent_info.node_index]};

        parent_node.SetVoxel(parent_info.child_index, voxel_value);
        DirtyContreeNode(parent_info.node_index);

        current_node = {&contree_headers[parent_info.node_index], &contree_data[parent_info.node_index]};
    }
}

Voxel VoxelManager::GetVoxel(uint32_t chunk, glm::uvec3 position) {
    uint32_t node = allocated_chunks[chunk].contree_node;
    
    glm::uvec3 chunk_width = glm::uvec3(Chunk::WIDTH);
    
    for (uint8_t depth; depth < ContreeNode::MAX_DEPTH; depth++) {
        chunk_width /= ContreeNode::WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = ContreeNode::GetIndex(node_position);
        if (contree_headers[node].IsVoxel(child_node_index)) {
            return static_cast<Voxel>(contree_data[node].GetVoxel(child_node_index));
        }
        node = contree_data[node].GetPtr(child_node_index);
    }
    // if nothing is found in the search (should be impossible)
    return VOXEL_EMPTY;
}

void VoxelManager::FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    glm::ivec3 fill_start = glm::min(start_position, end_position);
    glm::ivec3 fill_end   = glm::max(start_position, end_position);

    glm::ivec3 chunk_start = GetChunkPosition(fill_start);
    glm::ivec3 chunk_end = GetChunkPosition(fill_end) + 1;

    for (int32_t cx = chunk_start.x; cx < chunk_end.x; ++cx) {
        for (int32_t cy = chunk_start.y; cy < chunk_end.y; ++cy) {
            for (int32_t cz = chunk_start.z; cz < chunk_end.z; ++cz) {
                uint32_t c = GetChunkIndex(glm::ivec3(cx, cy, cz));
                if (c == POINTER_EMPTY) continue;
                FillVoxels(allocated_chunks[c].contree_node, 1, allocated_chunks[c].position * glm::ivec3(Chunk::WIDTH), fill_start, fill_end, voxel);
            }
        }
    }
}

bool Intersects(glm::ivec3 aMin, glm::ivec3 aMax, glm::ivec3 bMin, glm::ivec3 bMax) {
    return
        aMin.x <= bMax.x && aMax.x >= bMin.x &&
        aMin.y <= bMax.y && aMax.y >= bMin.y &&
        aMin.z <= bMax.z && aMax.z >= bMin.z;
}

bool FullyContains(glm::ivec3 outerMin, glm::ivec3 outerMax, glm::ivec3 innerMin, glm::ivec3 innerMax) {
    return
        innerMin.x >= outerMin.x && innerMax.x <= outerMax.x &&
        innerMin.y >= outerMin.y && innerMax.y <= outerMax.y &&
        innerMin.z >= outerMin.z && innerMax.z <= outerMax.z;
}

void VoxelManager::FillNodeUniform(uint32_t node_index, Voxel voxel) {
    glm::uvec3 i;
    for (i.x = 0; i.x < ContreeNode::WIDTH; i.x++)
        for (i.y = 0; i.y < ContreeNode::WIDTH; i.y++)
            for (i.z = 0; i.z < ContreeNode::WIDTH; i.z++) {
                uint16_t index = ContreeNode::GetIndex(i);
                if (!contree_headers[node_index].IsVoxel(index))
                    FreeContreeNode(contree_data[node_index].GetPtr(index));
                ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                node.SetVoxel(index, voxel);
            }
    DirtyContreeNode(node_index);
}

void VoxelManager::FillVoxels(uint32_t node_index, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    if (node_index == POINTER_EMPTY) return;
    if (depth > ContreeNode::MAX_DEPTH) return;

    uint32_t node_width = Chunk::WIDTH;
    for (uint8_t d = 0; d < depth; ++d) node_width /= ContreeNode::WIDTH;

    glm::uvec3 i;
    for (i.x = 0; i.x < ContreeNode::WIDTH; i.x++) {
        for (i.y = 0; i.y < ContreeNode::WIDTH; i.y++) {
            for (i.z = 0; i.z < ContreeNode::WIDTH; i.z++) {
                uint16_t index = ContreeNode::GetIndex(i);
                glm::ivec3 child_pos = node_position + glm::ivec3(i) * (int32_t)node_width;
                glm::ivec3 child_end = child_pos + glm::ivec3(node_width) - glm::ivec3(1);

                if (!Intersects(child_pos, child_end, start_position, end_position)) continue;

                if (FullyContains(start_position, end_position, child_pos, child_end)) {
                    if (!contree_headers[node_index].IsVoxel(index)) FreeContreeNode(contree_data[node_index].GetPtr(index));
                    ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                    node.SetVoxel(index, voxel);
                    DirtyContreeNode(node_index);
                    continue;
                }

                // partial coverage
                if (depth < ContreeNode::MAX_DEPTH) {
                    if (contree_headers[node_index].IsVoxel(index)) {
                        Voxel existing = contree_data[node_index].GetVoxel(index);
                        uint32_t child = AllocateContreeNode();
                        ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                        node.SetPtr(index, child);
                        DirtyContreeNode(node_index);
                        FillNodeUniform(child, existing);
                    }
                    FillVoxels(contree_data[node_index].GetPtr(index), depth + 1, child_pos, start_position, end_position, voxel);
                } else {
                    if (!contree_headers[node_index].IsVoxel(index)) FreeContreeNode(contree_data[node_index].GetPtr(index));
                    ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                    node.SetVoxel(index, voxel);
                    DirtyContreeNode(node_index);
                }
            }
        }
    }
}

void VoxelManager::GenerateChunkOccupancyMap() {
    if (allocated_chunks.empty()) {
        delete[] chunk_occupancy.chunks;
        chunk_occupancy.chunks = nullptr;
        return;
    }
    // Chunk-space bounds
    glm::ivec3 min = glm::ivec3(INT_MAX);
    glm::ivec3 max = glm::ivec3(INT_MIN);

    // Find global chunk bounds
    for (const Chunk& c : allocated_chunks) {
        min = glm::min(min, c.position);
        max = glm::max(max, c.position);
    }

    glm::ivec3 gridSize = (max - min) + glm::ivec3(1);
    
    size_t newSize = gridSize.x * gridSize.y * gridSize.z; 
    size_t oldSize = chunk_occupancy.get_size();

    // Resize occupancy vector if the size is different
    if (newSize != oldSize) {
        uint32_t *newMem = new uint32_t[newSize];
        if (!newMem) return;
        delete[] chunk_occupancy.chunks;
        chunk_occupancy.chunks = newMem;
        chunk_occupancy.size = gridSize;
    }
    
    chunk_occupancy.position = min;
    
    // Fill map with empty entries
    for (size_t i = 0; i < newSize; ++i) {
        chunk_occupancy.chunks[i] = POINTER_EMPTY;
    }

    // Fill occupancy map
    for (uint32_t i = 0; i < allocated_chunks.size(); ++i) {
        const Chunk& c = allocated_chunks[i];

        glm::ivec3 local = c.position - min;

        uint32_t index =
            local.x +
            local.y * gridSize.x +
            local.z * gridSize.x * gridSize.y;

        chunk_occupancy.chunks[index] = i;
    }
}

void VoxelManager::DirtyContreeNode(uint32_t node_index) {
    uint32_t page_index = node_index / 64;
    uint32_t bit_index  = node_index % 64;
    dirty_contree_nodes[page_index] |= (1ull << bit_index);
}

void VoxelManager::CleanContreeNodes() {
    for (uint64_t &clean : dirty_contree_nodes) {
        clean = 0;
    }
}

size_t VoxelManager::GetChunkDataAllocatedBytes() const {
    return contree_data.capacity() * sizeof(ContreeNode);
}