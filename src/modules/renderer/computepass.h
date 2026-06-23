#pragma once

#include "shaderpass.h"

#include <vector>
#include <functional>

#include "stdint.h"
#include "glm/vec3.hpp"

#include "modules/renderer/texture.h"
#include "modules/renderer/buffer.h"
#include "modules/renderer/sampler.h"

class ComputePass : public ShaderPass {
    public:
        using ShaderPass::ShaderPass;
        
        const uint32_t *spirv;
        size_t spirv_size;
        
        glm::ivec3 threadcount;
        
        std::vector<Texture*> readwrite_storage_textures;
        std::vector<Buffer*> readwrite_storage_buffers;
        std::vector<SamplerTextureBinding*> samplers;
        std::vector<Texture*> readonly_storage_textures;
        std::vector<Buffer*> readonly_storage_buffers;
        std::vector<Buffer*> uniform_buffers;

        std::vector<SDL_GPUStorageTextureReadWriteBinding> sdl_readwrite_storage_textures;
        std::vector<SDL_GPUStorageBufferReadWriteBinding> sdl_readwrite_storage_buffers;
        std::vector<SDL_GPUTextureSamplerBinding> sdl_samplers;
        std::vector<SDL_GPUTexture*> sdl_readonly_storage_textures;
        std::vector<SDL_GPUBuffer*> sdl_readonly_storage_buffers;
        std::vector<SDL_GPUBuffer*> sdl_uniform_buffers;

        void Create(void) override;
        void Destroy(void) override;
        void Execute(SDL_GPUCommandBuffer* cmd) override;
        void BuildSDLBuffers(void);

        // A lambda that returns the group invocation size
        std::function<glm::uvec3(const ComputePass&)> dispatchFunc;

        SDL_GPUComputePipeline* GetPipeline(void);
        
    private:
        SDL_GPUComputePipeline *computePipeline = nullptr;
};