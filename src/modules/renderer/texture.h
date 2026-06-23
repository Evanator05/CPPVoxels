#pragma once

#include "resource.h"

#include "glm/vec2.hpp"

class Texture : public Resource<SDL_GPUTexture> {
    public:
        using Resource::Resource;
        Texture(SDL_GPUDevice *device, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format);
        ~Texture() override;
    
        void Create(void) override;
        void Destroy(void) override;

        SDL_GPUTexture* GetGPU(void) override;

        void CreateFrom(SDL_GPUTexture *texture, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format);

        glm::ivec2 size{0, 0};
        SDL_GPUTextureUsageFlags usage{};
        SDL_GPUTextureFormat format{};
    private:
        bool owned = true;
};