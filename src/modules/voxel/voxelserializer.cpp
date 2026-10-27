#include "voxelserializer.h"
#include "voxelmanager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
constexpr size_t kChunkHeaderSize = offsetof(ChunkData, nodes);
constexpr size_t kWorldHeaderSize = offsetof(VoxelFileHeader, chunk_start_positions);

// Safely copy bytes into/out of a serialized byte array.
template<typename T>
T ReadAt(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Truncated voxel file");
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

void WriteBytes(std::vector<uint8_t>& bytes, size_t offset, const void* source, size_t count) {
    if (offset > bytes.size() || count > bytes.size() - offset)
        throw std::runtime_error("Voxel serialization buffer too small");
    std::memcpy(bytes.data() + offset, source, count);
}

struct ChunkFree {
    void operator()(ChunkData* chunk) const { std::free(chunk); }
};
} // namespace

void VoxelSerializer::SaveWorld() {
    const std::vector<uint8_t> bytes = SerializeWorld(GetModule<VoxelManager>());
    std::ofstream file("test.voxl", std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Failed to write test.voxl");
}

void VoxelSerializer::LoadWorld() {
    std::ifstream file("test.voxl", std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Failed to open test.voxl");

    const std::streampos end = file.tellg();
    if (end < 0)
        throw std::runtime_error("Failed to determine test.voxl size");

    std::vector<uint8_t> bytes(static_cast<size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Failed to read test.voxl");

    DeserializeWorld(std::move(bytes));
}

std::vector<uint8_t> VoxelSerializer::SerializeWorld(VoxelManager& vm) {
    if (vm.allocated_chunks.size() > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("Too many chunks to serialize");

    const uint32_t count = static_cast<uint32_t>(vm.allocated_chunks.size());
    const size_t table_size = static_cast<size_t>(count) * sizeof(uint64_t);
    std::vector<uint8_t> bytes(kWorldHeaderSize + table_size, 0);

    VoxelFileHeader header{};
    header.chunk_count = count;
    WriteBytes(bytes, 0, &header, kWorldHeaderSize);

    for (uint32_t i = 0; i < count; ++i) {
        std::vector<uint8_t> chunk = SerializeChunk(vm, i);
        const uint64_t offset = static_cast<uint64_t>(bytes.size());
        WriteBytes(bytes, kWorldHeaderSize + sizeof(uint64_t) * i,
                   &offset, sizeof(offset));
        bytes.insert(bytes.end(), chunk.begin(), chunk.end());
    }
    return bytes;
}

void VoxelSerializer::DeserializeWorld(std::vector<uint8_t> bytes) {
    VoxelManager& vm = GetModule<VoxelManager>();

    if (bytes.size() < kWorldHeaderSize)
        throw std::runtime_error("Invalid voxel file header");
    const VoxelFileHeader header = ReadAt<VoxelFileHeader>(bytes, 0);
    if (std::memcmp(header.magic, "VOXL", 4) != 0)
        throw std::runtime_error("Invalid voxel file magic");
    if (header.chunk_count > (bytes.size() - kWorldHeaderSize) / sizeof(uint64_t))
        throw std::runtime_error("Invalid voxel file chunk count");

    const size_t table_end = kWorldHeaderSize +
                             static_cast<size_t>(header.chunk_count) * sizeof(uint64_t);

    // Parse and validate the entire file before changing the current world.
    std::vector<std::unique_ptr<ChunkData, ChunkFree>> chunks;
    chunks.reserve(header.chunk_count);
    uint64_t previous_end = table_end;
    for (uint32_t i = 0; i < header.chunk_count; ++i) {
        const uint64_t start = ReadAt<uint64_t>(
            bytes, kWorldHeaderSize + static_cast<size_t>(i) * sizeof(uint64_t));
        const uint64_t finish = (i + 1 < header.chunk_count)
            ? ReadAt<uint64_t>(bytes, kWorldHeaderSize + static_cast<size_t>(i + 1) * sizeof(uint64_t))
            : static_cast<uint64_t>(bytes.size());
        if (start != previous_end || finish <= start || finish > bytes.size())
            throw std::runtime_error("Invalid voxel chunk offsets");
        previous_end = finish;

        std::vector<uint8_t> chunk_bytes(bytes.begin() + static_cast<size_t>(start),
                                          bytes.begin() + static_cast<size_t>(finish));
        std::unique_ptr<ChunkData, ChunkFree> chunk(DeserializeChunk(chunk_bytes));
        if (chunk->node_count == 0)
            throw std::runtime_error("Serialized chunk is missing root node");
        for (uint64_t j = 0; j < chunk->node_count; ++j) {
            const ContreeNodeData& node = chunk->nodes[j];
            for (uint32_t k = 0; k < 64; ++k) {
                if (!(node.isVoxelMask & (uint64_t{1} << k)) &&
                    node.node_data[k] >= chunk->node_count)
                    throw std::runtime_error("Invalid serialized child node index");
            }
        }
        chunks.push_back(std::move(chunk));
    }

    // Replace the world; AllocateChunk() also creates each root and updates occupancy.
    while (!vm.allocated_chunks.empty())
        vm.FreeChunk(static_cast<uint32_t>(vm.allocated_chunks.size() - 1));

    for (const auto& chunk_ptr : chunks) {
        const ChunkData& chunk = *chunk_ptr;
        const uint32_t chunk_index = vm.AllocateChunk(chunk.position);
        std::vector<uint32_t> indices(static_cast<size_t>(chunk.node_count));
        indices[0] = vm.allocated_chunks[chunk_index].contree_node;
        for (size_t j = 1; j < indices.size(); ++j)
            indices[j] = vm.AllocateContreeNode();

        for (size_t j = 0; j < indices.size(); ++j) {
            const ContreeNodeData& source = chunk.nodes[j];
            const uint32_t global_index = indices[j];

            // Don't simply memcpy the node and isVoxelMask: SetVoxel/SetPtr
            // must rebuild ContreeHeader::isSolidMask and related metadata.
            for (uint32_t k = 0; k < 64; ++k) {
                // Recreate the wrapper for each operation using current vector
                // addresses (all allocations for this chunk are already complete).
                ContreeNode node{&vm.contree_headers[global_index],
                                 &vm.contree_data[global_index]};
                const uint32_t value = source.node_data[k];
                if (source.isVoxelMask & (uint64_t{1} << k)) {
                    node.SetVoxel(k, static_cast<Voxel>(value));
                } else {
                    node.SetPtr(k, indices[value]);
                }
            }
            vm.DirtyContreeNode(global_index);
        }
        vm.DirtyChunk(chunk.position);
    }

    vm.chunk_occupancy_needs_rebuild = true;
    vm.GenerateChunkOccupancyMap();
}

std::vector<uint8_t> VoxelSerializer::SerializeChunk(ChunkData* chunk) {
    if (!chunk)
        throw std::runtime_error("Null chunk");
    const size_t bytes_count = kChunkHeaderSize +
                               static_cast<size_t>(chunk->node_count) * sizeof(ContreeNodeData);
    std::vector<uint8_t> bytes(bytes_count);
    std::memcpy(bytes.data(), chunk, bytes_count);
    return bytes;
}

std::vector<uint8_t> VoxelSerializer::SerializeChunk(VoxelManager& vm, uint32_t chunk_index) {
    const Chunk chunk = vm.allocated_chunks.at(chunk_index);
    std::vector<ContreeNodeData> nodes;

    // Each frame is one active path level; this creates true DFS preorder.
    struct Frame { uint32_t local_index; uint32_t next_child; };
    std::vector<Frame> stack;

    auto append_node = [&](uint32_t global_index) -> uint32_t {
        if (global_index >= vm.contree_headers.size() || global_index >= vm.contree_data.size())
            throw std::runtime_error("Invalid contree node index while saving");
        ContreeNodeData node{};
        node.isVoxelMask = vm.contree_headers[global_index].isVoxelMask;
        for (uint32_t i = 0; i < 64; ++i)
            node.node_data[i] = vm.contree_data[global_index].GetPtr(i);
        const uint32_t local_index = static_cast<uint32_t>(nodes.size());
        nodes.push_back(node);
        return local_index;
    };

    stack.push_back({append_node(chunk.contree_node), 0});
    while (!stack.empty()) {
        Frame& frame = stack.back();
        if (frame.next_child == 64) {
            stack.pop_back();
            continue;
        }
        const uint32_t local_index = frame.local_index;
        const uint32_t slot = frame.next_child++;
        if (nodes[local_index].isVoxelMask & (uint64_t{1} << slot))
            continue;

        const uint32_t global_child = nodes[local_index].node_data[slot];
        const uint32_t local_child = append_node(global_child);
        nodes[local_index].node_data[slot] = local_child;
        stack.push_back({local_child, 0});
    }

    const uint64_t count = nodes.size();
    const size_t byte_count = kChunkHeaderSize + nodes.size() * sizeof(ContreeNodeData);
    std::vector<uint8_t> bytes(byte_count, 0);
    WriteBytes(bytes, offsetof(ChunkData, position), &chunk.position, sizeof(chunk.position));
    WriteBytes(bytes, offsetof(ChunkData, node_count), &count, sizeof(count));
    if (!nodes.empty())
        WriteBytes(bytes, offsetof(ChunkData, nodes), nodes.data(),
                   nodes.size() * sizeof(ContreeNodeData));
    return bytes;
}

ChunkData* VoxelSerializer::DeserializeChunk(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kChunkHeaderSize)
        throw std::runtime_error("Invalid chunk header");
    const uint64_t count = ReadAt<uint64_t>(bytes, offsetof(ChunkData, node_count));
    if (count > (bytes.size() - kChunkHeaderSize) / sizeof(ContreeNodeData) ||
        kChunkHeaderSize + count * sizeof(ContreeNodeData) != bytes.size())
        throw std::runtime_error("Invalid chunk data size");

    ChunkData* chunk = static_cast<ChunkData*>(std::malloc(bytes.size()));
    if (!chunk)
        throw std::bad_alloc();
    std::memcpy(chunk, bytes.data(), bytes.size());
    return chunk;
}
