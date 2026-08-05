#pragma once

#include "engine.h"
#include "shaderpasses/computepass.h"
#include "resources/texture.h"

#include <vector>
#include <unordered_map>
#include <string>

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

class Renderer : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void SetVSync(bool enable);

        SDL_GPUDevice* GetDevice();

        std::vector<ShaderPass*> shaderPassOrder;

        Texture swapchainTexture{device};
    private:
        SDL_GPUDevice* device = nullptr; 
};