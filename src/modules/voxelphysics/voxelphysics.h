#pragma once

#include "engine.h"
#include "modules/voxel/voxel.h"

#include <glm/vec3.hpp>
#include <limits>

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f};
};

struct RayQuery {
    bool hit = false;
    glm::ivec3 position{0};
    float depth = (std::numeric_limits<float>::max)();
    glm::vec3 normal{0.0f};
    Voxel voxel{};
};

class VoxelPhysics : public EngineModule {
public:
    explicit VoxelPhysics(Engine* engine) : EngineModule(engine) {}
    RayQuery TraceWorld(Ray ray, float maxDepth = (std::numeric_limits<float>::max)());
};
