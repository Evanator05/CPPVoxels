#pragma once

#include "engine.h"
#include "modules/renderer/renderer.h"

class VoxelRenderer : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;
    private:
        SDL_GPUDevice *device = nullptr;
        Texture *display = nullptr;

        std::vector<ShaderPass*> shaderPasses{};
        std::vector<Texture*> textures{};
};