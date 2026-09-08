#pragma once

#include "../shaderpass.h"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>
#include "glm/vec3.hpp"
#include "../resources/texture.h"
#include "../resources/buffer.h"
#include "../resources/sampler.h"

class ComputePass : public ShaderPass {
public:
    using ShaderPass::ShaderPass;

    const uint32_t* spirv = nullptr;
    // Number of uint32_t words, NOT bytes (preserves the original API).
    size_t spirv_size = 0;
    glm::ivec3 threadcount{1, 1, 1};

    // Configure resource counts before Create(). Order within each list
    // determines the SDL slot. Shader SPIR-V bindings are:
    // Set 0: samplers, readonly textures, readonly buffers, consecutively.
    // Set 1: readwrite textures, readwrite buffers, consecutively.
    // Set 2: uniform_buffers, consecutively.
    std::vector<Texture*> readwrite_storage_textures;
    std::vector<Buffer*> readwrite_storage_buffers;
    std::vector<SamplerTextureBinding*> samplers;
    std::vector<Texture*> readonly_storage_textures;
    std::vector<Buffer*> readonly_storage_buffers;

    // Owned CPU bytes, not GPU Buffer pointers. Use SetUniformData().
    // Bytes must already match the shader's std140 uniform layout.
    std::vector<std::vector<Uint8>> uniform_buffers;

    std::vector<SDL_GPUStorageTextureReadWriteBinding> sdl_readwrite_storage_textures;
    std::vector<SDL_GPUStorageBufferReadWriteBinding> sdl_readwrite_storage_buffers;
    std::vector<SDL_GPUTextureSamplerBinding> sdl_samplers;
    std::vector<SDL_GPUTexture*> sdl_readonly_storage_textures;
    std::vector<SDL_GPUBuffer*> sdl_readonly_storage_buffers;

    void Create(void) override;
    void Destroy(void) override;
    void Execute(SDL_GPUCommandBuffer* cmd) override;
    void BuildSDLBuffers(void);

    void SetUniformData(Uint32 slot, const void* data, size_t byteSize);
    template<typename T>
    void SetUniformData(Uint32 slot, const T& data) {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Uniform data must be trivially copyable");
        SetUniformData(slot, &data, sizeof(T));
    }

    // Returns workgroup counts, not individual invocation counts.
    std::function<glm::uvec3(const ComputePass&)> dispatchFunc;
    SDL_GPUComputePipeline* GetPipeline(void);

private:
    std::array<size_t, 6> ResourceCounts() const;
    std::array<size_t, 6> pipelineCounts{};
    SDL_GPUComputePipeline* computePipeline = nullptr;
};
