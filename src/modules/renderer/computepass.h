#pragma once

#include <vector>

#include "stdint.h"
#include "glm/vec3.hpp"

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#include "modules/renderer/texture.h"

class ComputePass {
    public:
        ComputePass(SDL_GPUDevice *device);
        ~ComputePass();
        
        const uint32_t *spirv;
        size_t spirv_size;
        
        glm::ivec3 threadcount;
        
        std::vector<Texture*> readwrite_storage_textures;
        std::vector<SDL_GPUStorageBufferReadWriteBinding> readwrite_storage_buffers;
        std::vector<SDL_GPUTextureSamplerBinding> samplers;
        std::vector<SDL_GPUTexture*> readonly_storage_textures;
        std::vector<SDL_GPUBuffer*> readonly_storage_buffers;
        std::vector<SDL_GPUBuffer*> uniform_buffers;

        void CreatePipeline(void);
        void DestroyPipeline(void);
        SDL_GPUComputePipeline *GetPipeline(void);
        
    private:
        SDL_GPUDevice *device;
        SDL_GPUComputePipeline *computePipeline;
};