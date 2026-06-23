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

        void SetVSync(bool enable);

        SDL_GPUDevice* GetDevice();

        template <typename T>
        T* CreateShaderPass() {
            static_assert(std::is_base_of_v<ShaderPass, T>);
            T *pass = new T(device);
            shaderPasses.push_back(pass);
            return pass;
        }
        Texture* CreateTexture() {
            Texture *texture = new Texture(device);
            textures.push_back(texture);
            return texture;
        }
        Buffer* CreateBuffer() {
            Buffer *buffer = new Buffer(device);
            buffers.push_back(buffer);
            return buffer;
        }

        std::vector<ShaderPass*> shaderPasses;
        std::vector<Texture*> textures;
        std::vector<Buffer*> buffers;
        std::vector<ShaderPass*> shaderPassOrder;

        Texture swapchainTexture{device};
    private:
        SDL_GPUDevice* device = nullptr; 
};