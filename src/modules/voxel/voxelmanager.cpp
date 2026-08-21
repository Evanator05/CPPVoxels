#include "voxelmanager.h"

#include "glm/common.hpp"
#include <unordered_set>
#include <sstream>

#include "fixedstack/fixedstack.hpp"

void VoxelManager::Init() {
    ContreeDataBase::set_base(contree_data);
    AllocatedChunksBase::set_base(allocated_chunks);
    contree_data.reserve(10);
    free_contree_indicies.reserve(10);
    allocated_chunks.reserve(10);
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
    if (root == nullptr) return;

    ContreeNode &node = *root;

    if (node.isVoxelMask != CONTREE_VOXEL_MASK_FULL) {
        for (size_t i = 0; i < CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH * CONTREE_NODE_WIDTH; i++) {
            if (node.IsVoxel(i)) continue;
            FreeContreeNode(node.GetPtr(i));
        }
    }

    node.isVoxelMask = CONTREE_VOXEL_MASK_FULL;
    free_contree_indicies.push_back(root.offset);
}

Relptr<AllocatedChunksBase> VoxelManager::AllocateChunk(const glm::ivec3 position) {
    allocated_chunks.push_back({
        position,
        //CHUNK_FLAG_EXISTS,
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
            Voxel child_node_voxel = node->GetVoxel(child_node_index);
            if (child_node_voxel == voxel) return;

            Relptr<ContreeDataBase> new_node = AllocateContreeNode();
            node->SetPtr(child_node_index, new_node);
            
            for (uint8_t i = 0; i < CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH*CONTREE_NODE_WIDTH; i++) {
                new_node->SetVoxel(i, child_node_voxel); // fill new node with voxel data from parent
            }
        }

        stack.push({node, child_node_index});
        node = node->GetPtr(child_node_index);
    }

    uint8_t child_node_index = node->GetIndex(position);
    node->SetVoxel(child_node_index, voxel);

    ContreeNode* current_node = node;

    while (stack.size() > 0) {
        if (!current_node->IsUniform())
            break;

        Voxel voxel_value = current_node->GetVoxel(0);

        NodeStack parent_info = stack.pop();

        ContreeNode* parent_node = parent_info.node_index;

        FreeContreeNode(parent_node->GetPtr(parent_info.child_index));

        parent_node->SetVoxel(parent_info.child_index, voxel_value);

        current_node = parent_node;
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
            return static_cast<Voxel>(node->GetVoxel(child_node_index));
        }
        node = node->GetPtr(child_node_index);
    }
    // if nothing is found in the search (should be impossible)
    return VOXEL_EMPTY;
}

void VoxelManager::FillVoxels(glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    glm::ivec3 fill_start = glm::min(start_position, end_position);
    glm::ivec3 fill_end   = glm::max(start_position, end_position); // inclusive last voxel

    glm::ivec3 chunk_start = GetChunkPosition(fill_start);
    glm::ivec3 chunk_end   = GetChunkPosition(fill_end) + 1; // +1 just to make the loop bound exclusive

    for (int32_t cx = chunk_start.x; cx < chunk_end.x; ++cx) {
        for (int32_t cy = chunk_start.y; cy < chunk_end.y; ++cy) {
            for (int32_t cz = chunk_start.z; cz < chunk_end.z; ++cz) {
                Relptr<AllocatedChunksBase> c = GetChunkIndex(glm::ivec3(cx, cy, cz));
                if (c == nullptr) continue;
                FillVoxels(c->contree_node, 1, c->position * glm::ivec3(CHUNK_WIDTH), fill_start, fill_end, voxel);
            }
        }
    }
}

bool Intersects(glm::ivec3 aMin, glm::ivec3 aMax,
                 glm::ivec3 bMin, glm::ivec3 bMax)
{
    return
        aMin.x <= bMax.x && aMax.x >= bMin.x &&
        aMin.y <= bMax.y && aMax.y >= bMin.y &&
        aMin.z <= bMax.z && aMax.z >= bMin.z;
}

bool FullyContains(glm::ivec3 outerMin, glm::ivec3 outerMax,
                    glm::ivec3 innerMin, glm::ivec3 innerMax)
{
    return
        innerMin.x >= outerMin.x && innerMax.x <= outerMax.x &&
        innerMin.y >= outerMin.y && innerMax.y <= outerMax.y &&
        innerMin.z >= outerMin.z && innerMax.z <= outerMax.z;
}

// Sets every cell of a node to the same voxel, without further subdivision.
void VoxelManager::FillNodeUniform(Relptr<ContreeDataBase> node, Voxel voxel) {
    glm::uvec3 i;
    for (i.x = 0; i.x < CONTREE_NODE_WIDTH; i.x++)
        for (i.y = 0; i.y < CONTREE_NODE_WIDTH; i.y++)
            for (i.z = 0; i.z < CONTREE_NODE_WIDTH; i.z++) {
                uint16_t index = node->GetIndex(i);
                // FIX 1: pass the actual child Relptr (via GetPtr), not the raw slot index
                if (!node->IsVoxel(index)) FreeContreeNode(node->GetPtr(index));
                node->SetVoxel(index, voxel);
            }
}

void VoxelManager::FillVoxels(Relptr<ContreeDataBase> node, uint8_t depth, glm::ivec3 node_position, glm::ivec3 start_position, glm::ivec3 end_position, Voxel voxel) {
    if (node == nullptr) return;
    // FIX 3: allow execution at depth == CONTREE_MAX_DEPTH so the final level actually gets written
    if (depth > CONTREE_MAX_DEPTH) return;

    uint32_t node_width = CHUNK_WIDTH;
    for (uint8_t d = 0; d < depth; ++d) node_width /= CONTREE_NODE_WIDTH;

    glm::uvec3 i;
    for (i.x = 0; i.x < CONTREE_NODE_WIDTH; i.x++) {
        for (i.y = 0; i.y < CONTREE_NODE_WIDTH; i.y++) {
            for (i.z = 0; i.z < CONTREE_NODE_WIDTH; i.z++) {
                uint16_t index = node->GetIndex(i);
                glm::ivec3 child_pos = node_position + glm::ivec3(i) * (int32_t)node_width;
                glm::ivec3 child_end = child_pos + glm::ivec3(node_width) - glm::ivec3(1);

                if (!Intersects(child_pos, child_end, start_position, end_position)) continue;

                if (FullyContains(start_position, end_position, child_pos, child_end)) {
                    // FIX 1: pass the actual child Relptr (via GetPtr), not the raw slot index
                    if (!node->IsVoxel(index)) FreeContreeNode(node->GetPtr(index));
                    node->SetVoxel(index, voxel);
                    continue;
                }

                // partial coverage
                if (depth < CONTREE_MAX_DEPTH) {
                    if (node->IsVoxel(index)) {
                        Voxel existing = node->GetVoxel(index);
                        Relptr<ContreeDataBase> child = AllocateContreeNode();
                        node->SetPtr(index, child);
                        FillNodeUniform(child, existing);
                    }
                    FillVoxels(node->GetPtr(index), depth + 1, child_pos, start_position, end_position, voxel);
                } else {
                    // nothing left to subdivide (depth == CONTREE_MAX_DEPTH: each child is exactly one voxel)
                    // FIX 1: pass the actual child Relptr (via GetPtr), not the raw slot index
                    if (!node->IsVoxel(index)) FreeContreeNode(node->GetPtr(index));
                    node->SetVoxel(index, voxel);
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
            Voxel v = node.GetVoxel(i);
            if (!v.solid())
                ss << "VOXEL empty\n";
            else
                ss << "VOXEL " << v.to_string() << "\n";
        } else {
            ss << "NODE -> " << node.GetPtr(i) << "\n";
            DumpNode(nodes, node.GetPtr(i).offset, depth + 1, ss, visited);
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