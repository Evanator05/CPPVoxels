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
        Renderer(Engine *e);
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void CreateDisplayTextures(void);
        void UpdateDisplayTextures(void);

        void CreateComputePipeline(void);

        SDL_GPUDevice* GetDevice();
    private:
        SDL_GPUDevice* device = nullptr;
        std::unordered_map<std::string, ComputePass> computePasses;
        std::vector<ComputePass*> computePassOrder;
        std::unordered_map<std::string, Texture> displayTextures;
        std::vector<Texture*> displayTextureOrder;
};