#pragma once

#include "engine.h"
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

class VoxelPhysics : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;
    private:
        std::vector<PhysicsBody*> bodies;
};