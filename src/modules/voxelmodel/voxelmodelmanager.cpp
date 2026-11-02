#include "voxelmodelmanager.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
    template<typename T>
    void ReserveAppend(std::vector<T> &values, size_t count = 1) {
        if (count > values.max_size() - values.size())
            throw std::length_error("Model metadata exceeds vector limits");

        const size_t required = values.size() + count;
        if (required <= values.capacity())
            return;

        const size_t capacity = values.capacity();
        values.reserve(
            std::max(required, capacity + std::min(capacity, values.max_size() - capacity))
        );
    }
} // namespace

uint32_t VoxelModelManager::Volume(glm::uvec3 size) {
    uint64_t count = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (!size[axis])
            throw std::invalid_argument("Model dimensions must be positive");
        if (count > (UINT32_MAX / sizeof(Voxel)) / size[axis])
            throw std::length_error("Model voxel volume exceeds GPU buffer limits");

        count *= size[axis];
    }

    return uint32_t(count);
}

uint64_t VoxelModelManager::Handle(uint32_t slot, uint32_t generation) {
    return (uint64_t(generation) << 32) | slot;
}

void VoxelModelManager::Advance(uint32_t &generation) {
    if (++generation == 0)
        generation = 1;
}

bool VoxelModelManager::HasModel(uint64_t h) const {
    const uint32_t i = uint32_t(h);
    return i < modelSlots.size() && modelSlots[i].alive &&
           modelSlots[i].generation == uint32_t(h >> 32);
}

bool VoxelModelManager::HasInstance(uint64_t h) const {
    const uint32_t i = uint32_t(h);
    return i < instanceSlots.size() && instanceSlots[i].alive &&
           instanceSlots[i].generation == uint32_t(h >> 32);
}

uint32_t VoxelModelManager::AllocateVoxels(uint32_t count) {
    const auto best = freeBySize.lower_bound({count, 0});
    if (best != freeBySize.end()) {
        const uint32_t first = best->second;
        const uint32_t remaining = best->first - count;
        auto addressNode = freeByOffset.extract(first);
        auto sizeNode = freeBySize.extract(best);

        if (remaining) {
            // Reuse metadata nodes when splitting; no heap allocation needed.
            addressNode.key() = first + count;
            addressNode.mapped() = remaining;
            sizeNode.value() = {remaining, first + count};

            freeByOffset.insert(std::move(addressNode));
            freeBySize.insert(std::move(sizeNode));
        }

        return first;
    }

    if (uint64_t(voxels.size()) + count > UINT32_MAX / sizeof(Voxel))
        throw std::length_error("Shared model voxel buffer exceeds GPU limits");

    const uint32_t first = uint32_t(voxels.size());
    voxels.resize(voxels.size() + count);

    return first;
}

void VoxelModelManager::FreeVoxels(uint64_t model) {
    const uint32_t slot = uint32_t(model);
    uint32_t first = models[slot].voxelOffset;
    uint32_t count = modelSlots[slot].count;

    auto next = freeByOffset.lower_bound(first);
    auto previous = next == freeByOffset.begin() ? freeByOffset.end() : std::prev(next);
    const bool joinPrevious =
        previous != freeByOffset.end() && previous->first + previous->second == first;
    const bool joinNext = next != freeByOffset.end() && first + count == next->first;

    if (first + count == voxels.size()) {
        // The free tail needs no index nodes, and can absorb its left neighbor.
        if (joinPrevious) {
            first = previous->first;
            freeBySize.erase({previous->second, previous->first});
            freeByOffset.erase(previous);
        }

        voxels.resize(first);
        return;
    }

    if (joinPrevious || joinNext) {
        const auto reuse = joinPrevious ? previous : next;
        auto sizeNode = freeBySize.extract({reuse->second, reuse->first});
        auto addressNode = freeByOffset.extract(reuse);

        first = std::min(first, addressNode.key());
        count += addressNode.mapped();

        if (joinPrevious && joinNext) {
            count += next->second;
            freeBySize.erase({next->second, next->first});
            freeByOffset.erase(next);
        }

        addressNode.key() = first;
        addressNode.mapped() = count;
        sizeNode.value() = {count, first};

        freeByOffset.insert(std::move(addressNode));
        freeBySize.insert(std::move(sizeNode));
    } else {
        const auto inserted = freeByOffset.emplace(first, count).first;
        try {
            freeBySize.emplace(count, first);
        } catch (...) {
            freeByOffset.erase(inserted);
            throw;
        }
    }
}

uint64_t VoxelModelManager::CreateModel(glm::uvec3 size, std::span<const Voxel> source) {
    const uint32_t count = Volume(size);
    if (source.size() != count)
        throw std::invalid_argument("Voxel count does not match model dimensions");

    // A source span may refer into this pool. Remember its offset before pool
    // growth can relocate it; memmove also handles overlap with a reused hole.
    const uintptr_t sourceAddress = reinterpret_cast<uintptr_t>(source.data());
    const uintptr_t poolAddress = reinterpret_cast<uintptr_t>(voxels.data());
    const size_t poolBytes = voxels.size() * sizeof(Voxel);
    const bool fromPool =
        poolBytes && sourceAddress >= poolAddress && sourceAddress - poolAddress < poolBytes;
    const size_t sourceOffset = fromPool ? (sourceAddress - poolAddress) / sizeof(Voxel) : 0;

    if (fromPool && count > voxels.size() - sourceOffset)
        throw std::invalid_argument("Source span exceeds the model voxel pool");

    const bool recycled = !freeModels.empty();
    if (!recycled) {
        ReserveAppend(models);
        ReserveAppend(modelSlots);
    }

    ReserveAppend(dirtyVoxels);
    ReserveAppend(dirtyModels);

    const uint32_t first = AllocateVoxels(count);
    if (fromPool)
        source = {voxels.data() + sourceOffset, count};

    static_assert(std::is_trivially_copyable_v<Voxel>);
    std::memmove(voxels.data() + first, source.data(), size_t(count) * sizeof(Voxel));

    uint32_t slot;
    if (recycled) {
        slot = freeModels.back();
        freeModels.pop_back();
    } else {
        slot = uint32_t(models.size());
        models.emplace_back();
        modelSlots.emplace_back();
    }

    models[slot] = {{size.x, size.y, size.z}, first};
    modelSlots[slot].count = count;
    modelSlots[slot].alive = true;

    // Source storage stays complete, but air padding must not enlarge every
    // instance's ray search. This bound is computed once for the shared asset.
    auto &bounds = modelSlots[slot];
    bounds.firstInstance = Invalid;
    bounds.instanceCount = 0;
    bounds.occupied = false;
    bounds.minimum = glm::vec3(size);
    bounds.maximum = glm::vec3(0);

    for (uint32_t z = 0; z < size.z; ++z) {
        for (uint32_t y = 0; y < size.y; ++y) {
            for (uint32_t x = 0; x < size.x; ++x) {
                if (!voxels[first + (z * size.y + y) * size.x + x].solid())
                    continue;

                bounds.occupied = true;
                bounds.minimum = glm::min(bounds.minimum, glm::vec3(x, y, z));
                bounds.maximum = glm::max(bounds.maximum, glm::vec3(x + 1, y + 1, z + 1));
            }
        }
    }

    dirtyVoxels.push_back({first, count});
    dirtyModels.push_back(slot);
    ++dataRevision;

    return Handle(slot, modelSlots[slot].generation);
}

void VoxelModelManager::DestroyModel(uint64_t handle) {
    if (!HasModel(handle))
        return;

    const uint32_t slot = uint32_t(handle);

    // Reserve before mutating ownership. Free-range indexing can also allocate;
    // do it before removing any instances so failure leaves the model intact.
    ReserveAppend(freeModels);
    ReserveAppend(dirtyModels);
    ReserveAppend(freeInstances, modelSlots[slot].instanceCount);
    ReserveAppend(dirtyInstances, modelSlots[slot].instanceCount);
    FreeVoxels(handle);

    while (modelSlots[slot].firstInstance != Invalid) {
        const uint32_t instance = modelSlots[slot].firstInstance;
        DestroyInstance(Handle(instance, instanceSlots[instance].generation));
    }

    models[slot] = {};
    modelSlots[slot].alive = false;
    modelSlots[slot].count = 0;
    Advance(modelSlots[slot].generation);
    freeModels.push_back(slot);
    dirtyModels.push_back(slot);
    ++dataRevision;
}

uint64_t VoxelModelManager::CreateInstance(uint64_t model) {
    if (!HasModel(model))
        throw std::invalid_argument("Invalid or stale model handle");
    if (nextCreationOrder > UINT32_MAX)
        throw std::length_error("Model instance creation order exhausted");

    uint32_t slot;
    if (!freeInstances.empty()) {
        slot = freeInstances.back();
        freeInstances.pop_back();
    } else {
        if (instances.size() >= LeafBit)
            throw std::length_error("Too many model instances");
        slot = uint32_t(instances.size());
        ReserveAppend(instances);
        ReserveAppend(instanceSlots);
        ReserveAppend(gpuInstances);
        instances.emplace_back();
        instanceSlots.emplace_back();
        gpuInstances.emplace_back();
    }

    instances[slot] = {};
    auto &meta = instanceSlots[slot];
    meta.model = uint32_t(model);
    meta.alive = true;
    meta.cached = false;
    meta.inTree = false;
    meta.creationOrder = uint32_t(nextCreationOrder++);

    meta.previousInstance = Invalid;
    meta.nextInstance = modelSlots[meta.model].firstInstance;
    if (meta.nextInstance != Invalid)
        instanceSlots[meta.nextInstance].previousInstance = slot;

    modelSlots[meta.model].firstInstance = slot;
    ++modelSlots[meta.model].instanceCount;
    topologyDirty = true;

    return Handle(slot, meta.generation);
}

void VoxelModelManager::DestroyInstance(uint64_t h) {
    if (!HasInstance(h))
        return;

    const uint32_t slot = uint32_t(h);
    ReserveAppend(dirtyInstances);
    ReserveAppend(freeInstances);

    auto &meta = instanceSlots[slot];
    if (meta.previousInstance != Invalid)
        instanceSlots[meta.previousInstance].nextInstance = meta.nextInstance;
    else
        modelSlots[meta.model].firstInstance = meta.nextInstance;
    if (meta.nextInstance != Invalid)
        instanceSlots[meta.nextInstance].previousInstance = meta.previousInstance;

    meta.previousInstance = Invalid;
    meta.nextInstance = Invalid;
    --modelSlots[meta.model].instanceCount;

    meta.alive = false;
    meta.inTree = false;
    meta.cached = false;
    meta.leaf = Invalid;
    Advance(meta.generation);
    instances[slot].flags = 0;
    gpuInstances[slot] = {};

    dirtyInstances.push_back(slot);
    freeInstances.push_back(slot);
    topologyDirty = true;
}

ModelInstance *VoxelModelManager::GetInstance(uint64_t h) {
    return HasInstance(h) ? &instances[uint32_t(h)] : nullptr;
}

const ModelInstance *VoxelModelManager::GetInstance(uint64_t h) const {
    return HasInstance(h) ? &instances[uint32_t(h)] : nullptr;
}

bool VoxelModelManager::PackInstance(uint32_t slot) {
    auto &meta = instanceSlots[slot];
    const auto &instance = instances[slot];
    GPUModelInstance packed{};
    const auto &modelBounds = modelSlots[meta.model];
    bool valid = (instance.flags & ActiveFlag) != 0 && modelBounds.occupied;
    const glm::mat4 matrix(instance.transform);

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            valid &= std::isfinite(matrix[c][r]);
        }
    }

    const float determinant = glm::determinant(glm::mat3(matrix));
    valid &= std::isfinite(determinant) && determinant != 0;

    if (valid) {
        const glm::mat4 inverse = glm::inverse(matrix);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 4; ++column) {
                packed.inverseRows[row][column] = inverse[column][row];
                packed.worldRows[row][column] = matrix[column][row];
                valid &= std::isfinite(inverse[column][row]);
            }
        }

        const glm::vec3 half = (modelBounds.maximum - modelBounds.minimum) * 0.5f;
        const glm::vec3 center = glm::vec3(matrix * glm::vec4(modelBounds.minimum + half, 1));
        const glm::vec3 extent = glm::abs(glm::vec3(matrix[0])) * half.x +
                                 glm::abs(glm::vec3(matrix[1])) * half.y +
                                 glm::abs(glm::vec3(matrix[2])) * half.z;

        // World-aligned cells are sampled at their centers and rendered at full
        // unit extent, so the instance BVH must bound that snapped cell region.
        meta.minimum = glm::floor(center - extent);
        meta.maximum = glm::ceil(center + extent);
        for (int axis = 0; axis < 3; ++axis)
            valid &= std::isfinite(meta.minimum[axis]) && std::isfinite(meta.maximum[axis]) &&
                     meta.minimum[axis] >= float(INT32_MIN + 1024) &&
                     meta.maximum[axis] <= float(INT32_MAX - 1024);
    }
    if (valid) {
        packed.info[0] = meta.model;
        packed.info[1] = 1;
        packed.info[2] = uint32_t(instance.overlapPriority);
        packed.info[3] = meta.creationOrder;
    } else
        packed = {};

    if (std::memcmp(&packed, &gpuInstances[slot], sizeof(packed)) != 0) {
        gpuInstances[slot] = packed;
        dirtyInstances.push_back(slot);
    }

    return valid;
}

void VoxelModelManager::SetBounds(uint32_t index, glm::vec3 minimum, glm::vec3 maximum) {
    auto &node = nodes[index];
    for (int axis = 0; axis < 3; ++axis) {
        node.minimum[axis] = minimum[axis];
        node.maximum[axis] = maximum[axis];
    }
}

uint32_t VoxelModelManager::BuildNode(
    std::vector<uint32_t> &leaves,
    size_t first,
    size_t last,
    uint32_t parent
) {
    const uint32_t node = uint32_t(nodes.size());
    nodes.emplace_back();
    parents.push_back(parent);
    rightChildren.push_back(Invalid);

    if (last - first == 1) {
        const uint32_t slot = leaves[first];
        SetBounds(node, instanceSlots[slot].minimum, instanceSlots[slot].maximum);
        nodes[node].childOrInstance = LeafBit | slot;
        instanceSlots[slot].leaf = node;
    } else {
        glm::vec3 minimum(std::numeric_limits<float>::max());
        glm::vec3 maximum(-std::numeric_limits<float>::max());
        for (size_t i = first; i < last; ++i) {
            const auto &meta = instanceSlots[leaves[i]];
            const glm::vec3 center = meta.minimum * 0.5f + meta.maximum * 0.5f;
            minimum = glm::min(minimum, center);
            maximum = glm::max(maximum, center);
        }

        const glm::vec3 extent = maximum - minimum;
        const int axis =
            extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
        const size_t middle = first + (last - first) / 2;

        std::nth_element(
            leaves.begin() + first,
            leaves.begin() + middle,
            leaves.begin() + last,
            [&](uint32_t a, uint32_t b) {
                const float ca =
                    instanceSlots[a].minimum[axis] * 0.5f + instanceSlots[a].maximum[axis] * 0.5f;
                const float cb =
                    instanceSlots[b].minimum[axis] * 0.5f + instanceSlots[b].maximum[axis] * 0.5f;
                return ca == cb ? a < b : ca < cb;
            }
        );

        const uint32_t left = BuildNode(leaves, first, middle, node);
        const uint32_t right = BuildNode(leaves, middle, last, node);
        nodes[node].childOrInstance = left;
        rightChildren[node] = right;
        SetBounds(
            node,
            glm::min(glm::make_vec3(nodes[left].minimum), glm::make_vec3(nodes[right].minimum)),
            glm::max(glm::make_vec3(nodes[left].maximum), glm::make_vec3(nodes[right].maximum))
        );
    }

    nodes[node].escape = uint32_t(nodes.size());
    return node;
}

void VoxelModelManager::BuildBVH() {
    std::vector<uint32_t> leaves;
    for (uint32_t i = 0; i < instanceSlots.size(); ++i)
        if (instanceSlots[i].inTree)
            leaves.push_back(i);

    nodes.clear();
    parents.clear();
    rightChildren.clear();

    if (!leaves.empty()) {
        const size_t count = leaves.size() * 2 - 1;
        if (count > UINT32_MAX / sizeof(GPUModelBVHNode))
            throw std::length_error("Model BVH exceeds GPU buffer limits");
        nodes.reserve(count);
        parents.reserve(count);
        rightChildren.reserve(count);
        BuildNode(leaves, 0, leaves.size(), Invalid);
    }

    dirtyNodes.resize(nodes.size());
    for (uint32_t i = 0; i < nodes.size(); ++i)
        dirtyNodes[i] = i;

    topologyDirty = false;
    ++bvhBuildCount;
}

void VoxelModelManager::SynchronizeInstances() {
    std::vector<uint32_t> moved;
    priorityWinner = Invalid;

    for (uint32_t i = 0; i < instanceSlots.size(); ++i) {
        auto &meta = instanceSlots[i];
        if (!meta.alive)
            continue;

        const auto &current = instances[i];
        const bool poseChanged =
            !meta.cached ||
            std::memcmp(&current.transform, &meta.previous.transform, sizeof(current.transform)) !=
                0;

        if (poseChanged || current.flags != meta.previous.flags ||
            current.overlapPriority != meta.previous.overlapPriority) {
            const bool valid = PackInstance(i);
            if (valid != meta.inTree)
                topologyDirty = true;
            meta.inTree = valid;
            meta.previous = current;
            meta.cached = true;
            if (valid && poseChanged)
                moved.push_back(i);
        }

        if (meta.inTree && (priorityWinner == Invalid ||
                            current.overlapPriority > instances[priorityWinner].overlapPriority ||
                            (current.overlapPriority == instances[priorityWinner].overlapPriority &&
                             meta.creationOrder < instanceSlots[priorityWinner].creationOrder)))
            priorityWinner = i;
    }

    if (topologyDirty) {
        BuildBVH();
        return;
    }

    std::vector<uint32_t> ancestors;
    for (uint32_t slot : moved) {
        const auto &meta = instanceSlots[slot];
        const uint32_t leaf = meta.leaf;
        const auto previous = nodes[leaf];
        SetBounds(leaf, meta.minimum, meta.maximum);
        if (std::memcmp(&previous, &nodes[leaf], sizeof(previous)) == 0)
            continue;

        dirtyNodes.push_back(leaf);
        for (uint32_t p = parents[leaf]; p != Invalid; p = parents[p])
            ancestors.push_back(p);
    }

    if (ancestors.empty() && moved.empty())
        return;

    std::sort(ancestors.begin(), ancestors.end(), std::greater<uint32_t>());
    ancestors.erase(std::unique(ancestors.begin(), ancestors.end()), ancestors.end());

    for (uint32_t node : ancestors) {
        const auto &left = nodes[nodes[node].childOrInstance];
        const auto &right = nodes[rightChildren[node]];
        const auto previous = nodes[node];
        SetBounds(
            node,
            glm::min(glm::make_vec3(left.minimum), glm::make_vec3(right.minimum)),
            glm::max(glm::make_vec3(left.maximum), glm::make_vec3(right.maximum))
        );
        if (std::memcmp(&previous, &nodes[node], sizeof(previous)) != 0)
            dirtyNodes.push_back(node);
    }

    ++bvhRefitCount;
}

void VoxelModelManager::ClearDirtyUploads() {
    dirtyVoxels.clear();
    dirtyModels.clear();
    dirtyInstances.clear();
    dirtyNodes.clear();
}
