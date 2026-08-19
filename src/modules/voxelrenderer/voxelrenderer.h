#pragma once

#include "engine.h"
#include "modules/renderer/renderer.h"
#include "modules/renderer/resources/buffer.h"
#include "glm/vec3.hpp"
class VoxelRenderer : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;
    private:
        SDL_GPUDevice *device = nullptr;
        TypedBuffer<glm::vec3> *posBuffer = nullptr;
        glm::vec3 pos{};
};