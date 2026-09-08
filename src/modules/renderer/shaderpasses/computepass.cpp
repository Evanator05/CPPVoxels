#include "computepass.h"
#include <SDL3/SDL_error.h>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
Uint32 Count32(size_t value) {
    if (value > std::numeric_limits<Uint32>::max())
        throw std::runtime_error("ComputePass: size exceeds Uint32");
    return static_cast<Uint32>(value);
}

template<typename T>
auto RequireGPU(T* resource) -> decltype(resource->GetGPU()) {
    if (!resource)
        throw std::runtime_error("ComputePass: null resource");
    auto* gpu = resource->GetGPU();
    if (!gpu)
        throw std::runtime_error("ComputePass: resource has not been created");
    return gpu;
}
}

std::array<size_t, 6> ComputePass::ResourceCounts() const {
    return {{samplers.size(), readonly_storage_textures.size(),
             readonly_storage_buffers.size(), readwrite_storage_textures.size(),
             readwrite_storage_buffers.size(), uniform_buffers.size()}};
}

void ComputePass::SetUniformData(Uint32 slot, const void* data, size_t byteSize) {
    if (!data || !byteSize)
        throw std::invalid_argument("ComputePass: uniform data must not be empty");
    Count32(byteSize);
    if (slot > uniform_buffers.size())
        throw std::invalid_argument("ComputePass: add uniform slots consecutively");
    if (computePipeline && slot >= pipelineCounts[5])
        throw std::runtime_error("ComputePass: Destroy before adding uniform slots");
    std::vector<Uint8> bytes(byteSize);
    std::memcpy(bytes.data(), data, byteSize);
    if (slot == uniform_buffers.size())
        uniform_buffers.emplace_back();
    uniform_buffers[slot].swap(bytes);
}

void ComputePass::Create() {
    if (!spirv || !spirv_size ||
        spirv_size > std::numeric_limits<size_t>::max() / sizeof(*spirv))
        throw std::runtime_error("ComputePass: invalid SPIR-V code or word count");
    if (threadcount.x <= 0 || threadcount.y <= 0 || threadcount.z <= 0)
        throw std::runtime_error("ComputePass: thread counts must be positive");
    for (const auto& bytes : uniform_buffers) {
        if (bytes.empty())
            throw std::runtime_error("ComputePass: uniform slots must contain data");
        Count32(bytes.size());
    }

    SDL_GPUComputePipelineCreateInfo info{};
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.code = reinterpret_cast<const Uint8*>(spirv);
    info.code_size = spirv_size * sizeof(*spirv);
    info.entrypoint = "main";
    info.num_samplers = Count32(samplers.size());
    info.num_readonly_storage_textures = Count32(readonly_storage_textures.size());
    info.num_readonly_storage_buffers = Count32(readonly_storage_buffers.size());
    info.num_readwrite_storage_textures = Count32(readwrite_storage_textures.size());
    info.num_readwrite_storage_buffers = Count32(readwrite_storage_buffers.size());
    info.num_uniform_buffers = Count32(uniform_buffers.size());
    info.threadcount_x = static_cast<Uint32>(threadcount.x);
    info.threadcount_y = static_cast<Uint32>(threadcount.y);
    info.threadcount_z = static_cast<Uint32>(threadcount.z);

    auto* newPipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (!newPipeline)
        throw std::runtime_error(std::string("Failed to create compute pipeline: ") + SDL_GetError());
    Destroy();
    computePipeline = newPipeline;
    pipelineCounts = ResourceCounts();
}

void ComputePass::Destroy() {
    if (computePipeline)
        SDL_ReleaseGPUComputePipeline(device, computePipeline);
    computePipeline = nullptr;
    pipelineCounts = {};
}

void ComputePass::Execute(SDL_GPUCommandBuffer* cmd) {
    if (!cmd || !computePipeline)
        throw std::runtime_error("ComputePass: command buffer and pipeline are required");

    const glm::uvec3 groups = dispatchFunc ? dispatchFunc(*this) : glm::uvec3(1, 1, 1);
    if (ResourceCounts() != pipelineCounts)
        throw std::runtime_error("ComputePass: resource counts changed; call Create() again");
    if (!groups.x || !groups.y || !groups.z)
        return;

    BuildSDLBuffers();
    // Validate before recording any uniform pushes or opening the pass.
    for (const auto& bytes : uniform_buffers) {
        if (bytes.empty())
            throw std::runtime_error("ComputePass: missing uniform data");
        Count32(bytes.size());
    }
    for (size_t i = 0; i < uniform_buffers.size(); ++i) {
        const auto& bytes = uniform_buffers[i];
        SDL_PushGPUComputeUniformData(cmd, static_cast<Uint32>(i),
                                     bytes.data(), static_cast<Uint32>(bytes.size()));
    }

    SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(
        cmd,
        sdl_readwrite_storage_textures.empty() ? nullptr : sdl_readwrite_storage_textures.data(),
        static_cast<Uint32>(sdl_readwrite_storage_textures.size()),
        sdl_readwrite_storage_buffers.empty() ? nullptr : sdl_readwrite_storage_buffers.data(),
        static_cast<Uint32>(sdl_readwrite_storage_buffers.size()));
    if (!pass)
        throw std::runtime_error(std::string("Failed to begin compute pass: ") + SDL_GetError());

    SDL_BindGPUComputePipeline(pass, computePipeline);
    // SDL slots start at zero within EACH category. SDL applies the
    // SPIR-V binding offsets; do not add those offsets here.
    if (!sdl_samplers.empty())
        SDL_BindGPUComputeSamplers(pass, 0, sdl_samplers.data(),
                                  static_cast<Uint32>(sdl_samplers.size()));
    if (!sdl_readonly_storage_textures.empty())
        SDL_BindGPUComputeStorageTextures(pass, 0, sdl_readonly_storage_textures.data(),
                                         static_cast<Uint32>(sdl_readonly_storage_textures.size()));
    if (!sdl_readonly_storage_buffers.empty())
        SDL_BindGPUComputeStorageBuffers(pass, 0, sdl_readonly_storage_buffers.data(),
                                        static_cast<Uint32>(sdl_readonly_storage_buffers.size()));
    SDL_DispatchGPUCompute(pass, groups.x, groups.y, groups.z);
    SDL_EndGPUComputePass(pass);
}

void ComputePass::BuildSDLBuffers() {
    sdl_readwrite_storage_textures.resize(readwrite_storage_textures.size());
    for (size_t i = 0; i < readwrite_storage_textures.size(); ++i) {
        SDL_GPUStorageTextureReadWriteBinding binding{};
        binding.texture = RequireGPU(readwrite_storage_textures[i]);
        binding.mip_level = 0;
        binding.layer = 0;
        binding.cycle = false;
        sdl_readwrite_storage_textures[i] = binding;
    }
    sdl_readwrite_storage_buffers.resize(readwrite_storage_buffers.size());
    for (size_t i = 0; i < readwrite_storage_buffers.size(); ++i) {
        SDL_GPUStorageBufferReadWriteBinding binding{};
        binding.buffer = RequireGPU(readwrite_storage_buffers[i]);
        binding.cycle = false;
        sdl_readwrite_storage_buffers[i] = binding;
    }
    sdl_samplers.resize(samplers.size());
    for (size_t i = 0; i < samplers.size(); ++i) {
        if (!samplers[i])
            throw std::runtime_error("ComputePass: null sampler binding");
        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = RequireGPU(samplers[i]->texture);
        binding.sampler = RequireGPU(samplers[i]->sampler);
        sdl_samplers[i] = binding;
    }
    sdl_readonly_storage_textures.resize(readonly_storage_textures.size());
    for (size_t i = 0; i < readonly_storage_textures.size(); ++i)
        sdl_readonly_storage_textures[i] = RequireGPU(readonly_storage_textures[i]);
    sdl_readonly_storage_buffers.resize(readonly_storage_buffers.size());
    for (size_t i = 0; i < readonly_storage_buffers.size(); ++i)
        sdl_readonly_storage_buffers[i] = RequireGPU(readonly_storage_buffers[i]);
}

SDL_GPUComputePipeline* ComputePass::GetPipeline() {
    return computePipeline;
}
