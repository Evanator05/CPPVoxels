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

        template<typename ResourceType>
        ResourceType* CreateResource() {
            ResourceType* resource = new ResourceType(device);

            if constexpr (std::is_base_of_v<IResource, ResourceType>)
                resources.push_back(resource);

            if constexpr (std::is_base_of_v<IExecutableResource, ResourceType>)
                executableResources.push_back(resource);

            return resource;
        }

        template<typename PassType>
        PassType* CreateShaderPass() {
            PassType* pass = new PassType(device);
            shaderPasses.push_back(pass);
            return pass;
        }

        SDL_GPUDevice* GetDevice();

        Texture swapchainTexture{device};
    private:
        SDL_GPUDevice* device = nullptr; 

        std::vector<IResource*> resources;
        std::vector<IExecutableResource*> executableResources;
        std::vector<ShaderPass*> shaderPasses;
};