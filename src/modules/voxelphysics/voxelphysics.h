#pragma once

#include "engine.h"

#include "modules/voxel/voxel.h"

#include "glm/vec3.hpp"
#include <vector>

enum class VoxelType {
    NONE,
    CORNER,
    EDGE,
    CENTER
};

struct PhysicsBody {
    glm::ivec3 size{};
    VoxelType data[];
};

struct Ray {

};

struct RayQuery {
    bool hit;
    glm::ivec3 position;
    float depth;
    glm::vec3 normal;
    Voxel voxel;
};

class VoxelPhysics : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        RayQuery TraceWorld(Ray ray);

    private:
        std::vector<PhysicsBody*> bodies;
};