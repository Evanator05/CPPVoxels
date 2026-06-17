#include "computepass.h"

#include <stdexcept>

void ComputePass::Create() {
    SDL_GPUComputePipelineCreateInfo createInfo{};

    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)spirv;
    createInfo.code_size = spirv_size*sizeof(*spirv);
    createInfo.entrypoint = "main";

    createInfo.num_readwrite_storage_textures = readwrite_storage_textures.size();
    createInfo.num_samplers = samplers.size();
    createInfo.num_readonly_storage_textures = readonly_storage_textures.size();
    createInfo.num_readonly_storage_buffers = readonly_storage_buffers.size();
    createInfo.num_readwrite_storage_buffers = readwrite_storage_buffers.size();
    createInfo.num_uniform_buffers = uniform_buffers.size();

    createInfo.threadcount_x = threadcount.x;
    createInfo.threadcount_y = threadcount.y;
    createInfo.threadcount_z = threadcount.z;

    computePipeline = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!computePipeline) throw std::runtime_error("Failed to create compute pipeline");
}

void ComputePass::Destroy() {
    if (computePipeline) SDL_ReleaseGPUComputePipeline(device, computePipeline);
    computePipeline = nullptr;
}

void ComputePass::Execute(SDL_GPUCommandBuffer* cmd) {
    BuildSDLBuffers();
    SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, sdl_readwrite_storage_textures.data(), sdl_readwrite_storage_textures.size(), sdl_readwrite_storage_buffers.data(), sdl_readwrite_storage_buffers.size());
    SDL_BindGPUComputePipeline(cpass, GetPipeline());
    
    // bind buffers
    if (sdl_samplers.size()) SDL_BindGPUComputeSamplers(cpass, 0, sdl_samplers.data(), sdl_samplers.size());
    if (sdl_readonly_storage_textures.size()) SDL_BindGPUComputeStorageTextures(cpass, 0, sdl_readonly_storage_textures.data(), sdl_readonly_storage_textures.size());
    if (sdl_readonly_storage_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, sdl_readonly_storage_buffers.data(), sdl_readonly_storage_buffers.size());
    if (sdl_uniform_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, sdl_uniform_buffers.data(), sdl_uniform_buffers.size());

    glm::uvec3 groups{1,1,1};
    if (dispatchFunc) {
        groups = dispatchFunc(*this);
    }
    
    SDL_DispatchGPUCompute(cpass, groups.x, groups.y, groups.z);
    SDL_EndGPUComputePass(cpass);
}

void ComputePass::BuildSDLBuffers() {
    sdl_readwrite_storage_textures.resize(readwrite_storage_textures.size());
    for (size_t i = 0; i < readwrite_storage_textures.size(); ++i) {
        SDL_GPUStorageTextureReadWriteBinding b{};
        b.texture = readwrite_storage_textures[i]->GetGPU();
        sdl_readwrite_storage_textures[i] = b;
    }

    sdl_readwrite_storage_buffers.resize(readwrite_storage_buffers.size());
    for (size_t i = 0; i < readwrite_storage_buffers.size(); ++i) {
        SDL_GPUStorageBufferReadWriteBinding b{};
        b.buffer = readwrite_storage_buffers[i]->GetGPU();
        sdl_readwrite_storage_buffers[i] = b;
    }
  
    sdl_samplers.resize(samplers.size());
    for (size_t i = 0; i < samplers.size(); ++i) {
        SDL_GPUTextureSamplerBinding b{};
        b.texture = samplers[i]->texture->GetGPU();
        b.sampler = samplers[i]->sampler->GetGPU();
        sdl_samplers[i] = b;
    }
    
    sdl_readonly_storage_textures.resize(readonly_storage_textures.size());
    for (size_t i = 0; i < readonly_storage_textures.size(); ++i) {
        sdl_readonly_storage_textures[i] = readonly_storage_textures[i]->GetGPU();
    }
    
    sdl_readonly_storage_buffers.resize(readonly_storage_buffers.size());
    for (size_t i = 0; i < readonly_storage_buffers.size(); ++i) {
        sdl_readonly_storage_buffers[i] = readonly_storage_buffers[i]->GetGPU();
    }

    sdl_uniform_buffers.resize(uniform_buffers.size());
    for (size_t i = 0; i < uniform_buffers.size(); ++i) {
        sdl_uniform_buffers[i] = uniform_buffers[i]->GetGPU();
    }

    std::vector<Texture*> samplers;
    std::vector<SDL_GPUTextureSamplerBinding> sdl_samplers;
}

SDL_GPUComputePipeline* ComputePass::GetPipeline() {
    return computePipeline;
}