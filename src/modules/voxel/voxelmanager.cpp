#include "voxelmanager.h"

#include "glm/common.hpp"
#include <unordered_set>
#include <sstream>

#include "fixedstack/fixedstack.hpp"

void VoxelManager::Init() {
    ContreeDataBase::set_base(contree_data);
    AllocatedChunksBase::set_base(allocated_chunks);
}

void VoxelManager::Process() {

}

void VoxelManager::Shutdown() {
    delete[] chunk_occupancy.chunks;
    contree_data.reserve(0);
    free_contree_indicies.reserve(0);
    allocated_chunks.reserve(0);
}

Relptr<ContreeDataBase> VoxelManager::AllocateContreeNode() {
    uint32_t data_index;
    if (free_contree_indicies.empty()) {
        contree_data.push_back({});
        data_index = static_cast<uint32_t>(contree_data.size() - 1);
    } else {
        data_index = free_contree_indicies.back();
        free_contree_indicies.pop_back();
    }
    contree_data[data_index] = {};
    contree_data[data_index].isVoxelMask = CONTREE_VOXEL_MASK_FULL; // default to all empty voxels
    return data_index;
}

void VoxelManager::FreeContreeNode(Relptr<ContreeDataBase> root) {
    FixedStack<Relptr<ContreeDataBase>, CONTREE_MAX_DEPTH> stack;

    stack.push(root);

    while (stack.size()) {
        Relptr<ContreeDataBase> index = stack.pop();

        ContreeNode &node = *index;

        if (node.isVoxelMask != CONTREE_VOXEL_MASK_FULL) {
            for (size_t i = 0; i < CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH; i++) {
                if (node.IsVoxel(i)) continue;
                stack.push(node.GetChildPtr(i));
            }
        }

        node.isVoxelMask = CONTREE_VOXEL_MASK_FULL;
        free_contree_indicies.push_back(index.offset);
    }
}

Relptr<AllocatedChunksBase> VoxelManager::AllocateChunk(const glm::ivec3 position) {
    allocated_chunks.push_back({
        position,
        CHUNK_FLAG_EXISTS,
        AllocateContreeNode()
    });
    return allocated_chunks.size() - 1;
}

void VoxelManager::FreeChunk(Relptr<AllocatedChunksBase> chunk) {
    FreeContreeNode(chunk->contree_node);
    allocated_chunks[chunk.offset] = allocated_chunks.back();
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

    return chunk_occupancy.chunks[index].offset;
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
    Relptr<AllocatedChunksBase> chunk = chunk_index;
    if (chunk == nullptr) return;
    SetVoxel(chunk, local_position, voxel);
}

Voxel VoxelManager::GetVoxel(glm::ivec3 world_position) {
    glm::ivec3 chunk_position = GetChunkPosition(world_position);
    glm::ivec3 local_position = world_position - chunk_position * glm::ivec3(CHUNK_WIDTH);
    uint32_t chunk_index = GetChunkIndex(chunk_position);
    Relptr<AllocatedChunksBase> chunk = chunk_index;
    if (chunk == nullptr) return VOXEL_EMPTY;
    return GetVoxel(chunk, local_position);
}

void VoxelManager::SetVoxel(Relptr<AllocatedChunksBase> chunk, glm::uvec3 position, Voxel voxel) {
    struct NodeStack {
        Relptr<ContreeDataBase> node_index;
        uint8_t child_index;
    };
    FixedStack<NodeStack, CONTREE_MAX_DEPTH> stack;

    Relptr<ContreeDataBase> node = chunk->contree_node;
    
    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);

    stack.push({node, 0});

    for (uint8_t depth = 0; depth < CONTREE_MAX_DEPTH - 1; depth++) { // depth - 1 because we dont need to allocate/check on the last layer we just want to set a voxel in it
        chunk_width /= CONTREE_NODE_WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = node->GetIndex(node_position);

        if (node->IsVoxel(child_node_index)) {
            Voxel child_node_voxel = node->GetChildVoxel(child_node_index);
            if (child_node_voxel == voxel) return;

            Relptr<ContreeDataBase> new_node = AllocateContreeNode();
            node->SetPtr(child_node_index, new_node);
            
            for (uint8_t i = 0; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
                new_node->SetVoxel(i, child_node_voxel); // fill new node with voxel data from parent
            }
        }

        stack.push({node, child_node_index});
        node = node->GetChildPtr(child_node_index);
    }

    uint8_t child_node_index = node->GetIndex(position);
    node->SetVoxel(child_node_index, voxel);

    // merge uniform node regions
    while (stack.size() > 1) {
        NodeStack current = stack.pop();
        ContreeNode* current_node = current.node_index;
        
        // if the node isnt uniform no reason to continue
        if (!current_node->IsUniform())
            break;
        
        Voxel voxel_value = current_node->GetChildVoxel(0);
        FreeContreeNode(current.node_index);

        NodeStack parent = stack.top();
        ContreeNode* parent_node = parent.node_index;

        parent_node->SetVoxel(current.child_index, voxel_value);
    }
}

Voxel VoxelManager::GetVoxel(Relptr<AllocatedChunksBase> chunk, glm::uvec3 position) {
    Relptr<ContreeDataBase> node = chunk->contree_node;
    
    glm::uvec3 chunk_width = glm::uvec3(CHUNK_WIDTH);
    

    for (uint8_t depth; depth < CONTREE_MAX_DEPTH; depth++) {
        chunk_width /= CONTREE_NODE_WIDTH;

        glm::uvec3 node_position = (position / chunk_width);
        position -= node_position * chunk_width;

        uint8_t child_node_index = node->GetIndex(node_position);
        if (node->IsVoxel(child_node_index)) {
            return static_cast<Voxel>(node->GetChildVoxel(child_node_index));
        }
        node = node->GetChildPtr(child_node_index);
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

    for (uint32_t cx = start_position.x; cx < end_position.x; ++cx) {
        for (uint32_t cy = start_position.y; cy < end_position.y; ++cy) {
            for (uint32_t cz = start_position.z; cz < end_position.z; ++cz) {
                Relptr<AllocatedChunksBase> c = GetChunkIndex(glm::ivec3(cx, cy, cz));
                FillVoxels(c->contree_node, 1, c->position*glm::ivec3(CHUNK_WIDTH), start_position, end_position, voxel);
            }
        }
    }
}

void VoxelManager::FillVoxels(Relptr<ContreeDataBase> node, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    uint16_t node_width = CHUNK_WIDTH / pow(CONTREE_NODE_WIDTH, depth);
    for (uint8_t x = 0; x < CONTREE_NODE_WIDTH; x++) {
        for (uint8_t y = 0; y < CONTREE_NODE_WIDTH; y++) {
            for (uint8_t z = 0; z < CONTREE_NODE_WIDTH; z++) {
                


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
        Relptr<AllocatedChunksBase> *newMem = new Relptr<AllocatedChunksBase>[newSize];
        if (!newMem) return;
        delete[] chunk_occupancy.chunks;
        chunk_occupancy.chunks = newMem;
        chunk_occupancy.size = gridSize;
    }
    
    chunk_occupancy.position = min;
    
    // Fill map with empty entries
    for (size_t i = 0; i < newSize; ++i) {
        chunk_occupancy.chunks[i] = nullptr;
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

        ss << std::string((depth + 1) * 2, ' ')
           << "[" << i << "] ";

        if (isVoxel) {
            Voxel v = node.GetChildVoxel(i);
            if (!v.solid())
                ss << "VOXEL empty\n";
            else
                ss << "VOXEL " << v.to_string() << "\n";
        } else {
            ss << "NODE -> " << node.GetChildPtr(i) << "\n";
            DumpNode(nodes, node.GetChildPtr(i).offset, depth + 1, ss, visited);
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