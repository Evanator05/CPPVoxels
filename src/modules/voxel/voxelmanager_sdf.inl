#pragma once

#include <cmath>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "voxelmanager.h"

inline uint32_t VoxelManager::AllocateSDFNode(Voxel initial_voxel) {
    uint32_t node_index = AllocateContreeNode();
    ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
    for (uint32_t i = 0; i < ContreeNode::WIDTH * ContreeNode::WIDTH * ContreeNode::WIDTH; ++i)
        node.SetVoxel(i, initial_voxel);
    return node_index;
}

template<uint32_t CellWidth, class SDF>
bool VoxelManager::FillSDFNode(uint32_t& node_index, Voxel initial_voxel, glm::vec3 position, Voxel voxel, SDF& sdf) {
    static_assert(CellWidth >= 1);
    static_assert(CellWidth == 1 || CellWidth % ContreeNode::WIDTH == 0);

    constexpr float half_width = float(CellWidth) * 0.5f;
    constexpr float bound = 0.86602545f * float(CellWidth - 1);

    bool changed = false;
    bool dirty = false;

    for (uint32_t z = 0; z < ContreeNode::WIDTH; ++z) {
        for (uint32_t y = 0; y < ContreeNode::WIDTH; ++y) {
            for (uint32_t x = 0; x < ContreeNode::WIDTH; ++x) {
                uint16_t index = ContreeNode::GetIndex(glm::uvec3(x, y, z));
                bool is_voxel = node_index == POINTER_EMPTY || contree_headers[node_index].IsVoxel(index);
                Voxel existing = initial_voxel;
                uint32_t child = POINTER_EMPTY;

                if (node_index != POINTER_EMPTY) {
                    if (is_voxel)
                        existing = contree_data[node_index].GetVoxel(index);
                    else
                        child = contree_data[node_index].GetPtr(index);
                }

                if (is_voxel && existing == voxel)
                    continue;

                glm::vec3 child_position = position + glm::vec3(float(x * CellWidth), float(y * CellWidth), float(z * CellWidth));
                float distance = sdf(child_position + glm::vec3(half_width));

                if constexpr (CellWidth == 1) {
                    if (!(distance <= 0.0f))
                        continue;
                } else {
                    if (distance > bound)
                        continue;

                    if (!(distance <= -bound)) {
                        if (!FillSDFNode<CellWidth / ContreeNode::WIDTH>(child, existing, child_position, voxel, sdf))
                            continue;

                        changed = true;
                        ContreeNode child_node{&contree_headers[child], &contree_data[child]};

                        if (!child_node.IsUniform()) {
                            if (is_voxel) {
                                if (node_index == POINTER_EMPTY)
                                    node_index = AllocateSDFNode(initial_voxel);

                                ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                                node.SetPtr(index, child);
                                dirty = true;
                            }
                            continue;
                        }
                    }
                }

                if (child != POINTER_EMPTY)
                    FreeContreeNode(child);

                if (node_index == POINTER_EMPTY)
                    node_index = AllocateSDFNode(initial_voxel);

                ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
                node.SetVoxel(index, voxel);
                dirty = true;
                changed = true;
            }
        }
    }

    if (dirty)
        DirtyContreeNode(node_index);

    return changed;
}

template<class SDF>
void VoxelManager::FillSDFRoot(uint32_t node_index, glm::vec3 position, Voxel voxel, SDF& sdf) {
    if (node_index == POINTER_EMPTY)
        return;

    if (contree_headers[node_index].IsVoxel(0) && contree_data[node_index].GetVoxel(0) == voxel) {
        ContreeNode node{&contree_headers[node_index], &contree_data[node_index]};
        if (node.IsUniform())
            return;
    }

    constexpr float bound = 0.86602545f * float(Chunk::WIDTH - 1);
    float distance = sdf(position + glm::vec3(float(Chunk::WIDTH) * 0.5f));

    if (distance > bound)
        return;

    if (distance <= -bound) {
        FillNodeUniform(node_index, voxel);
        return;
    }

    FillSDFNode<Chunk::WIDTH / ContreeNode::WIDTH>(node_index, VOXEL_EMPTY, position, voxel, sdf);
}

template<class SDF>
void VoxelManager::FillSDF(Voxel voxel, SDF&& sdf) {
    for (const Chunk& chunk : allocated_chunks) {
        FillSDFRoot(chunk.contree_node, glm::vec3(chunk.position) * float(Chunk::WIDTH), voxel, sdf);
    }
}

template<class SDF>
void VoxelManager::FillSDF(uint32_t node_index, Voxel voxel, SDF&& sdf) {
    FillSDFRoot(node_index, glm::vec3(0.0f), voxel, sdf);
}

template<class SDF>
void VoxelManager::FillSDF(Voxel voxel, SDF&& sdf, glm::ivec3 sdf_min, glm::ivec3 sdf_max) {
    if (allocated_chunks.empty() ||
        sdf_min.x > sdf_max.x || sdf_min.y > sdf_max.y || sdf_min.z > sdf_max.z)
        return;

    glm::ivec3 first = glm::max(GetChunkPosition(sdf_min), chunk_occupancy.position);
    glm::ivec3 last = glm::min(GetChunkPosition(sdf_max),
        chunk_occupancy.position + glm::ivec3(chunk_occupancy.size) - glm::ivec3(1));

    if (first.x > last.x || first.y > last.y || first.z > last.z)
        return;

    double candidate_count =
        double(int64_t(last.x) - first.x + 1) *
        double(int64_t(last.y) - first.y + 1) *
        double(int64_t(last.z) - first.z + 1);

    if (candidate_count >= double(allocated_chunks.size())) {
        for (const Chunk& chunk : allocated_chunks) {
            glm::ivec3 p = chunk.position;
            if (p.x < first.x || p.y < first.y || p.z < first.z ||
                p.x > last.x || p.y > last.y || p.z > last.z)
                continue;
            FillSDFRoot(chunk.contree_node, glm::vec3(p) * float(Chunk::WIDTH), voxel, sdf);
        }
        return;
    }

    for (int32_t z = first.z; z <= last.z; ++z) {
        for (int32_t y = first.y; y <= last.y; ++y) {
            for (int32_t x = first.x; x <= last.x; ++x) {
                glm::ivec3 p(x, y, z);
                uint32_t c = GetChunkIndex(p);
                if (c == POINTER_EMPTY)
                    continue;
                FillSDFRoot(allocated_chunks[c].contree_node, glm::vec3(p) * float(Chunk::WIDTH), voxel, sdf);
            }
        }
    }
}

inline void VoxelManager::FillSphere(glm::vec3 position, float radius, Voxel voxel) {
    if (!(radius >= 0.0f) || !std::isfinite(radius))
        return;

    glm::ivec3 first(glm::ceil(position - glm::vec3(radius) - glm::vec3(0.5f)));
    glm::ivec3 last(glm::floor(position + glm::vec3(radius) - glm::vec3(0.5f)));

    FillSDF(voxel, [position, radius](glm::vec3 pos) {
        return glm::length(pos - position) - radius;
    }, first, last);
}
