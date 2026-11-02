#pragma once

#include "engine.h"
#include "modules/voxel/voxel.h"

#include <glm/mat4x3.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include <span>
#include <map>
#include <set>
#include <vector>

// One shared, complete x-fastest voxel volume per model (including air/interior).
struct alignas(16) GPUVoxelModel {
    uint32_t dimensions[3]{};
    uint32_t voxelOffset = 0;
};

struct ModelInstance {
    glm::mat4x3 transform{1.0f};
    uint32_t flags = 1;

    // Higher values win shared world cells. Equal priorities favor the older
    // instance. Every model instance takes precedence over the environment.
    int32_t overlapPriority = 0;
};

struct alignas(16) GPUModelInstance {
    float inverseRows[3][4]{};
    uint32_t info[4]{}; // model slot, enabled, signed overlap priority, creation order
    float worldRows[3][4]{};
};

// Preorder BVH. Internal nodes point to their first child; leaves
// have LeafBit | instanceSlot. escape is the first node after this subtree.
struct alignas(16) GPUModelBVHNode {
    float minimum[3]{};
    uint32_t childOrInstance = 0;
    float maximum[3]{};
    uint32_t escape = 0;
};

static_assert(sizeof(Voxel) == 4 && sizeof(GPUVoxelModel) == 16);
static_assert(sizeof(GPUModelInstance) == 112 && sizeof(GPUModelBVHNode) == 32);

struct alignas(16) GPUModelSceneParameters {
    uint32_t counts[4]{};  // BVH nodes, model slots, instance slots, highest-priority instance
    uint32_t offsets[4]{}; // descriptor, instance, BVH byte offsets, model-grid alignment
};

static_assert(sizeof(GPUModelSceneParameters) == 32);

class VoxelModelManager : public EngineModule {
public:
    using EngineModule::EngineModule;
    static constexpr uint32_t Invalid = UINT32_MAX;
    static constexpr uint32_t ActiveFlag = 1;
    static constexpr uint32_t LeafBit = 0x80000000u;

    enum class GridAlignment : uint32_t {
        World = 0,
        Model = 1
    };

    struct VoxelRange {
        uint32_t first;
        uint32_t count;
    };

    uint64_t CreateModel(glm::uvec3 dimensions, std::span<const Voxel> voxels);
    void DestroyModel(uint64_t model);
    uint64_t CreateInstance(uint64_t model);
    void DestroyInstance(uint64_t instance);
    ModelInstance *GetInstance(uint64_t instance);
    const ModelInstance *GetInstance(uint64_t instance) const;

    // Detects mutable transforms/flags/priority, packs inverse transforms, and refits
    // changed BVH leaves. Only membership changes rebuild BVH topology.
    void SynchronizeInstances();
    void ClearDirtyUploads();

    const auto &GetVoxels() const {
        return voxels;
    }

    const auto &GetModels() const {
        return models;
    }

    const auto &GetGPUInstances() const {
        return gpuInstances;
    }

    const auto &GetBVHNodes() const {
        return nodes;
    }

    const auto &GetDirtyVoxelRanges() const {
        return dirtyVoxels;
    }

    const auto &GetDirtyModels() const {
        return dirtyModels;
    }

    const auto &GetDirtyInstances() const {
        return dirtyInstances;
    }

    const auto &GetDirtyBVHNodes() const {
        return dirtyNodes;
    }

    uint64_t GetDataRevision() const {
        return dataRevision;
    }

    uint64_t GetBVHBuildCount() const {
        return bvhBuildCount;
    }

    uint64_t GetBVHRefitCount() const {
        return bvhRefitCount;
    }

    uint32_t GetPriorityWinner() const {
        return priorityWinner;
    }

    GridAlignment GetGridAlignment() const {
        return gridAlignment;
    }

    void SetGridAlignment(GridAlignment alignment) {
        gridAlignment = alignment;
    }

private:
    struct ModelSlot {
        uint32_t generation = 1;
        uint32_t count = 0;
        uint32_t firstInstance = Invalid;
        uint32_t instanceCount = 0;

        bool alive = false;
        bool occupied = false;

        glm::vec3 minimum{};
        glm::vec3 maximum{};
    };

    struct InstanceSlot {
        uint32_t generation = 1;
        uint32_t model = Invalid;
        uint32_t leaf = Invalid;
        uint32_t creationOrder = 0;
        uint32_t previousInstance = Invalid;
        uint32_t nextInstance = Invalid;

        bool alive = false;
        bool cached = false;
        bool inTree = false;

        ModelInstance previous{};
        glm::vec3 minimum{};
        glm::vec3 maximum{};
    };

    std::vector<Voxel> voxels;
    std::vector<GPUVoxelModel> models;
    std::vector<ModelSlot> modelSlots;

    std::vector<ModelInstance> instances;
    std::vector<InstanceSlot> instanceSlots;
    std::vector<GPUModelInstance> gpuInstances;
    std::vector<uint32_t> freeModels;
    std::vector<uint32_t> freeInstances;

    // Address ordering supports neighbor coalescing; size ordering supports
    // best-fit allocation. Live voxel offsets never move when holes are freed.
    std::map<uint32_t, uint32_t> freeByOffset;
    std::set<std::pair<uint32_t, uint32_t>> freeBySize;

    std::vector<VoxelRange> dirtyVoxels;
    std::vector<uint32_t> dirtyModels;
    std::vector<uint32_t> dirtyInstances;
    std::vector<uint32_t> dirtyNodes;

    std::vector<GPUModelBVHNode> nodes;
    std::vector<uint32_t> parents;
    std::vector<uint32_t> rightChildren;

    bool topologyDirty = false;

    uint64_t dataRevision = 1;
    uint64_t bvhBuildCount = 0;
    uint64_t bvhRefitCount = 0;
    uint64_t nextCreationOrder = 0;
    uint32_t priorityWinner = Invalid;
    GridAlignment gridAlignment = GridAlignment::Model;

    bool HasModel(uint64_t handle) const;
    bool HasInstance(uint64_t handle) const;
    static uint32_t Volume(glm::uvec3 dimensions);
    static uint64_t Handle(uint32_t slot, uint32_t generation);
    static void Advance(uint32_t &generation);
    uint32_t AllocateVoxels(uint32_t count);
    void FreeVoxels(uint64_t model);
    bool PackInstance(uint32_t slot);
    void BuildBVH();
    uint32_t BuildNode(std::vector<uint32_t> &leaves, size_t first, size_t last, uint32_t parent);
    void SetBounds(uint32_t node, glm::vec3 minimum, glm::vec3 maximum);
};
