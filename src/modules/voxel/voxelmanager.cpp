#include "voxelmanager.h"

#include "glm/common.hpp"
#include <unordered_set>
#include <sstream>
void VoxelManager::Init() {

}

void VoxelManager::Process() {

}

void VoxelManager::Shutdown() {
    delete[] chunk_occupancy.chunk_indices;
    contree_data.reserve(0);
    free_contree_indicies.reserve(0);
    allocated_chunks.reserve(0);
}

uint32_t VoxelManager::AllocateContreeNode(void) {
    uint32_t data_index;
    if (free_contree_indicies.empty()) {
        contree_data.push_back({});
        data_index = static_cast<uint32_t>(contree_data.size() - 1);
    } else {
        data_index = free_contree_indicies.back();
        free_contree_indicies.pop_back();
    }
    contree_data[data_index] = {};
    contree_data[data_index].isVoxelMask = (uint64_t)(-1ULL); // default to all empty voxels
    return data_index;
}

void VoxelManager::FreeContreeNode(uint32_t rootIndex) {
    std::vector<uint32_t> stack(CONTREE_MAX_DEPTH);
    stack.push_back(rootIndex);

    while (!stack.empty()) {
        uint32_t index = stack.back();
        stack.pop_back();

        ContreeNode &node = contree_data[index];

        if (node.isVoxelMask != (uint64_t)(-1ULL)) {
            for (size_t i = 0; i < CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH; i++) {
                if (node.IsVoxel(i)) continue;
                stack.push_back(node.GetChildValue(i));
            }
        }

        node.isVoxelMask = (uint64_t)(-1ULL);
        free_contree_indicies.push_back(index);
    }
}

uint32_t VoxelManager::AllocateChunk(const glm::ivec3 position) {
    allocated_chunks.push_back({
        position,
        CHUNK_FLAG_EXISTS,
        AllocateContreeNode()
    });
    return allocated_chunks.size() - 1;
}

void VoxelManager::FreeChunk(const uint32_t index) {
    Chunk *chunk = GetChunkFromIndex(index);
    FreeContreeNode(chunk->chunk_data_index);

    allocated_chunks[index] = allocated_chunks.back();
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

    return chunk_occupancy.chunk_indices[index];
}

Chunk* VoxelManager::GetChunkFromIndex(const uint32_t index) {
    if (index >= allocated_chunks.size())
        return nullptr;
    return &allocated_chunks[index];
}

ContreeNode* VoxelManager::GetNodeFromIndex(uint32_t index) {
    if (index >= contree_data.size())
        return nullptr;
    return &contree_data[index];
}

glm::ivec3 VoxelManager::GetChunkPosition(glm::ivec3 world_position) {
    return glm::ivec3(
        world_position.x >= 0 ? world_position.x / CHUNK_WIDTH : (world_position.x - CHUNK_WIDTH + 1) / CHUNK_WIDTH,
        world_position.y >= 0 ? world_position.y / CHUNK_WIDTH : (world_position.y - CHUNK_WIDTH + 1) / CHUNK_WIDTH,
        world_position.z >= 0 ? world_position.z / CHUNK_WIDTH : (world_position.z - CHUNK_WIDTH + 1) / CHUNK_WIDTH
    );
}

// global space getting and setting voxels
void VoxelManager::SetVoxel(glm::ivec3 world_position, Voxel voxel) {
    glm::ivec3 chunk_position = GetChunkPosition(world_position);
    glm::ivec3 local_position = world_position - chunk_position * glm::ivec3(CHUNK_WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    Chunk *c = GetChunkFromIndex(chunk_index);
    if (c == nullptr) return;
    SetVoxel(c, local_position, voxel);
}

Voxel VoxelManager::GetVoxel(glm::ivec3 world_position) {
    glm::ivec3 chunk_position = GetChunkPosition(world_position);
    glm::ivec3 local_position = world_position - chunk_position * glm::ivec3(CHUNK_WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    Chunk *c = GetChunkFromIndex(chunk_index);
    if (c == nullptr) return VOXEL_EMPTY;
    return GetVoxel(c, local_position);
}

void VoxelManager::SetVoxel(Chunk *chunk, glm::uvec3 position, Voxel voxel) {
    uint32_t node_index = chunk->chunk_data_index;
    ContreeNode *node = GetNodeFromIndex(node_index);
    
    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);

    for (uint8_t depth; depth < CONTREE_MAX_DEPTH - 1; depth++) {
        chunk_width /= CONTREE_NODE_WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = node->GetIndex(node_position);
        bool child_node_is_voxel = node->IsVoxel(child_node_index);
        uint32_t child_node_data = node->GetChildValue(child_node_index);
        uint32_t new_node_data = child_node_data;
        if (child_node_is_voxel) {
            uint32_t new_node_index = AllocateContreeNode();
            node = GetNodeFromIndex(node_index); // regather node from its index because allocating new nodes can leave the pointer dangling
            node->SetValue(child_node_index, false, new_node_index);
            
            ContreeNode *new_node = GetNodeFromIndex(new_node_index);
            
            for (uint8_t i = 0; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
                new_node->SetValue(i, true, child_node_data); // fill new node with voxel data from parent
            }
            new_node_data = new_node_index;
        }

        node_index = new_node_data;
        node = GetNodeFromIndex(node_index);
    }

    uint8_t child_node_index = node->GetIndex(position);
    node->SetValue(child_node_index, true, voxel.data);
}

Voxel VoxelManager::GetVoxel(const Chunk *chunk, glm::uvec3 position) {
    ContreeNode *node = GetNodeFromIndex(chunk->chunk_data_index);
    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);
    for (uint8_t depth; depth < CONTREE_MAX_DEPTH; depth++) {
        chunk_width /= CONTREE_NODE_WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = node->GetIndex(node_position);
        bool child_node_is_voxel = node->IsVoxel(child_node_index);
        uint32_t child_node_data = node->GetChildValue(child_node_index);
        if (child_node_is_voxel) {
            return (Voxel)child_node_data;
        }
        node = GetNodeFromIndex(child_node_data);
    }
    // if nothing is found in the search (should be impossible)
    return VOXEL_EMPTY;
}

void VoxelManager::FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    glm::ivec3 chunk_start = glm::min(start_position, end_position);
    glm::ivec3 chunk_end   = glm::max(start_position, end_position);
    start_position = chunk_start;
    end_position = chunk_end;
    chunk_start = GetChunkPosition(start_position);
    chunk_end = GetChunkPosition(end_position);

    // loop through all affected chunks
    // loop through each node and check if its
    // convert node bounds to world positions
    //  A) No Coverage
    //  B) Full Coverage
    //  C) Partial Coverage
    // I can skip nodes in A
    // I can set the entire leaf to my value in B
    // I need to allocate a new child node and try again in C
}

void VoxelManager::GenerateChunkOccupancyMap() {
    if (allocated_chunks.empty()) {
        delete[] chunk_occupancy.chunk_indices;
        chunk_occupancy.chunk_indices = nullptr;
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
        delete[] chunk_occupancy.chunk_indices;
        chunk_occupancy.chunk_indices = newMem;
        chunk_occupancy.size = gridSize;
    }
    
    chunk_occupancy.position = min;
    
    // Fill map with empty entries
    for (size_t i = 0; i < newSize; ++i) {
        chunk_occupancy.chunk_indices[i] = POINTER_EMPTY;
    }

    // Fill occupancy map
    for (uint32_t i = 0; i < allocated_chunks.size(); ++i) {
        const Chunk& c = allocated_chunks[i];

        glm::ivec3 local = c.position - min;

        uint32_t index =
            local.x +
            local.y * gridSize.x +
            local.z * gridSize.x * gridSize.y;

        chunk_occupancy.chunk_indices[index] = i;
    }
    
}

size_t VoxelManager::GetChunkDataAllocatedBytes() const {
    return contree_data.capacity() * sizeof(ContreeNode);
}

static void DumpNode(
    std::vector<ContreeNode>& nodes,
    uint32_t index,
    int depth,
    std::stringstream& ss,
    std::unordered_set<uint32_t>& visited)
{
    if (index == UINT32_MAX || index >= nodes.size()) {
        ss << std::string(depth * 2, ' ')
           << "[invalid/null]\n";
        return;
    }

    if (visited.contains(index)) {
        ss << std::string(depth * 2, ' ')
           << "[cycle node " << index << "]\n";
        return;
    }

    visited.insert(index);

    ContreeNode& node = nodes[index];

    ss << std::string(depth * 2, ' ')
       << "Node " << index
       << " (depth " << depth << ")"
       << " mask=0x" << std::hex << node.isVoxelMask << std::dec
       << "\n";

    const int N = CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH;

    for (int i = 0; i < N; i++) {
        bool isVoxel = node.IsVoxel(i);
        uint32_t value = node.GetChildValue(i);

        ss << std::string((depth + 1) * 2, ' ')
           << "[" << i << "] ";

        if (isVoxel) {
            Voxel v = (Voxel)value;
            if (!v.solid())
                ss << "VOXEL empty\n";
            else
                ss << "VOXEL " << value << "\n";
        } else {
            ss << "NODE -> " << value << "\n";
            DumpNode(nodes, value, depth + 2, ss, visited);
        }
    }
}

std::string VoxelManager::DumpContreeGraph(uint32_t rootIndex)
{
    std::stringstream ss;
    std::unordered_set<uint32_t> visited;

    DumpNode(contree_data, rootIndex, 0, ss, visited);

    return ss.str();
}