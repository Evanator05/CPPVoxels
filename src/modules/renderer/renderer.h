#pragma once

#include "engine.h"
#include "computepass.h"
#include "texture.h"

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

        void CreateDisplayTextures(void);
        void UpdateDisplayTextures(glm::ivec2 size);

        void CreateComputePipeline(void);

        void SetVSync(bool enable);

        SDL_GPUDevice* GetDevice();
    private:
        SDL_GPUDevice* device = nullptr;
        std::unordered_map<std::string, ComputePass> computePasses;
        std::vector<ComputePass*> computePassOrder;
        std::unordered_map<std::string, Texture> displayTextures;
        std::vector<Texture*> displayTextureOrder;
};