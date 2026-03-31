#include "computepass.h"

#include <stdexcept>

ComputePass::ComputePass(SDL_GPUDevice *device) {
    this->device = device;
}

ComputePass::~ComputePass() {
    DestroyPipeline();
}

void ComputePass::CreatePipeline() {
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

void ComputePass::DestroyPipeline() {
    if (computePipeline) SDL_ReleaseGPUComputePipeline(device, computePipeline);
    computePipeline = nullptr;
}

SDL_GPUComputePipeline* ComputePass::GetPipeline() {
    return computePipeline;
}