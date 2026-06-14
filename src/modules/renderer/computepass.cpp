#include "computepass.h"

#include <stdexcept>


void ComputePass::Execute(SDL_GPUCommandBuffer* cmd) {
    
    std::vector<SDL_GPUStorageTextureReadWriteBinding> rw_bindings(readwrite_storage_textures.size());

    for (size_t i = 0; i < readwrite_storage_textures.size(); i++) {
        rw_bindings[i].texture = readwrite_storage_textures[i]->GetGPUTexture();
    }

    SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, rw_bindings.data(), rw_bindings.size(), readwrite_storage_buffers.data(), readwrite_storage_buffers.size());
    SDL_BindGPUComputePipeline(cpass, GetPipeline());
    
    // bind buffers
    if (samplers.size()) SDL_BindGPUComputeSamplers(cpass, 0, samplers.data(), samplers.size());
    if (readonly_storage_textures.size()) SDL_BindGPUComputeStorageTextures(cpass, 0, readonly_storage_textures.data(), readonly_storage_textures.size());
    if (readonly_storage_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, readonly_storage_buffers.data(), readonly_storage_buffers.size());
    if (uniform_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, uniform_buffers.data(),uniform_buffers.size());

    glm::uvec3 groups{1,1,1};
    if (dispatchFunc) {
        groups = dispatchFunc(*this);
    }
    
    SDL_DispatchGPUCompute(cpass, groups.x, groups.y, groups.z);
    SDL_EndGPUComputePass(cpass);
}

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

SDL_GPUComputePipeline* ComputePass::GetPipeline() {
    return computePipeline;
}