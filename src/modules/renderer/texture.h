#pragma once

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#include "glm/vec2.hpp"

class Texture {
    public:
        Texture(SDL_GPUDevice *device);
        Texture(SDL_GPUDevice *device, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format);
        ~Texture();

        void CreateTexture(void);
        void DestroyTexture(void);

        SDL_GPUTexture* GetGPUTexture(void);

        glm::ivec2 size{0, 0};
        SDL_GPUTextureUsageFlags usage{};
        SDL_GPUTextureFormat format{};
    private:
        SDL_GPUDevice *device = nullptr;
        SDL_GPUTexture *texture = nullptr;
};