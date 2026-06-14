#pragma once

#include "shaderpass.h"

#include <vector>
#include <functional>

#include "stdint.h"
#include "glm/vec3.hpp"

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#include "modules/renderer/texture.h"

class ComputePass : ShaderPass {
    public:
        using ShaderPass::ShaderPass;
        
        const uint32_t *spirv;
        size_t spirv_size;
        
        glm::ivec3 threadcount;
        
        std::vector<Texture*> readwrite_storage_textures;
        std::vector<SDL_GPUStorageBufferReadWriteBinding> readwrite_storage_buffers;
        std::vector<SDL_GPUTextureSamplerBinding> samplers;
        std::vector<SDL_GPUTexture*> readonly_storage_textures;
        std::vector<SDL_GPUBuffer*> readonly_storage_buffers;
        std::vector<SDL_GPUBuffer*> uniform_buffers;

        void Create(void) override;
        void Destroy(void) override;
        void Execute(SDL_GPUCommandBuffer* cmd) override;

        std::function<glm::uvec3(const ComputePass&)> dispatchFunc;

        SDL_GPUComputePipeline* GetPipeline(void);
        
    private:
        SDL_GPUComputePipeline *computePipeline = nullptr;
};