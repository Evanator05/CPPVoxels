#include <iostream>
#include <stdexcept>

#include "i_graphics.h"
#include "i_video.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "i_gui.h"

#include "shaders/main.h"
#include "shaders/halfresdepth.h"

SDL_GPUDevice* device = nullptr;

// textures
typedef struct _RenderTextures {
    enum Type { HalfDepth, FullDepth, IndexMap, Display, Count };
    SDL_GPUTexture* tex[Count]{};

    SDL_GPUTexture*& halfResDepth() { return tex[HalfDepth]; }
    SDL_GPUTexture*& fullResDepth() { return tex[FullDepth]; }
    SDL_GPUTexture*& indexMap()     { return tex[IndexMap]; }
    SDL_GPUTexture*& display()      { return tex[Display]; }

    void cleanup() {
        for (int i = 0; i < Count; i++) {
            if (tex[i]) SDL_ReleaseGPUTexture(device, tex[i]);
        }
    }
} RenderTextures;

RenderTextures renderTextures{};

// pipelines
SDL_GPUComputePipeline* mainDisplayPipeline = nullptr;

int winw, winh;

void graphics_init(void) {
    SDL_GetWindowSize(window, &winw, &winh);

    initDevice();
    initDisplayTextures();
    initComputePipeline();

    //SDL_SetGPUAllowedFramesInFlight(device, 3);
    //SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
}

void graphics_cleanup(void) {
    if (mainDisplayPipeline) SDL_ReleaseGPUComputePipeline(device, mainDisplayPipeline);

    renderTextures.cleanup();    

    if (device) SDL_DestroyGPUDevice(device);
}

void graphics_update(void) {
    drawFrame();
}

void initDevice() {
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!device) std::cerr << "Failed to create GPU device\n";
    SDL_ClaimWindowForGPUDevice(device, window);
}

void initDisplayTextures() {
    // halfres depth
    SDL_GPUTextureCreateInfo createInfo{};
    createInfo.width = (Uint32)winw/2;
    createInfo.height = (Uint32)winh/2;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;

    renderTextures.halfResDepth() = SDL_CreateGPUTexture(device, &createInfo);
    if (!renderTextures.halfResDepth()) std::cerr << "Failed to create halfres depth Texture\n";

    // fullres depth
    createInfo = {};
    createInfo.width = (Uint32)winw;
    createInfo.height = (Uint32)winh;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;

    renderTextures.fullResDepth() = SDL_CreateGPUTexture(device, &createInfo);
    if (!renderTextures.fullResDepth()) std::cerr << "Failed to create fullres depth Texture\n";
    
    // index map
    createInfo = {};
    createInfo.width = (Uint32)winw;
    createInfo.height = (Uint32)winh;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R32G32_UINT;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;

    renderTextures.indexMap() = SDL_CreateGPUTexture(device, &createInfo);
    if (!renderTextures.indexMap()) std::cerr << "Failed to create fullres depth Texture\n";

    // display texture
    createInfo = {};
    createInfo.width = (Uint32)winw;
    createInfo.height = (Uint32)winh;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    renderTextures.display() = SDL_CreateGPUTexture(device, &createInfo);
    if (!renderTextures.display()) std::cerr << "Failed to create Display Texture\n";
}

void initComputePipeline() {
    SDL_GPUComputePipelineCreateInfo createInfo{};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)halfresdepth_spirv;
    createInfo.code_size = std::size(halfresdepth_spirv)*sizeof(*halfresdepth_spirv);
    createInfo.entrypoint = "main";

    createInfo.num_readwrite_storage_textures = 1;
    createInfo.num_samplers = 0;
    createInfo.num_readonly_storage_textures = 0;
    createInfo.num_readonly_storage_buffers = GPUBUFFERCOUNT;
    createInfo.num_readwrite_storage_buffers = 0;
    createInfo.num_uniform_buffers = 0;

    createInfo.threadcount_x = 16;
    createInfo.threadcount_y = 16;
    createInfo.threadcount_z = 1;

    mainDisplayPipeline = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!mainDisplayPipeline) std::cerr << "Failed to create compute pipeline\n";
}

void drawFrame(void) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    
    // half res depth pass

    // upscale to full res primary ray starting depths
    
    // full res voxel index/visible voxel pass + maybe some other per voxel information

    // visible voxel lighting pass

    // main draw pass / combine color with lighting pass
    SDL_GPUStorageTextureReadWriteBinding rw{};
    rw.texture = renderTextures.display();
    rw.mip_level = 0;
    rw.layer = 0;

    SDL_GPUComputePass* cpass = SDL_BeginGPUComputePass(cmd, &rw, 1, nullptr, 0);
    SDL_BindGPUComputePipeline(cpass, mainDisplayPipeline);

    // bind buffers
    SDL_BindGPUComputeStorageBuffers(cpass, 0, gpubuffers_getVoxelBuffers(), GPUBUFFERCOUNT);

    const int localSize = 16;
    Uint32 groupsX = (winw+localSize-1)/localSize;
    Uint32 groupsY = (winh+localSize-1)/localSize;
    SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
    SDL_EndGPUComputePass(cpass);

    SDL_GPUTexture *swapTex = nullptr;
    Uint32 sw = 0, sh = 0;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapTex, &sw, &sh);

    // copy image into swaptexture
    if (swapTex) {
        SDL_GPUTextureLocation sourceLoc{};
        sourceLoc.texture = renderTextures.display();
        SDL_GPUTextureLocation destLoc{};
        destLoc.texture = swapTex;
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_CopyGPUTextureToTexture(copyPass, &sourceLoc, &destLoc, sw, sh, 1, false);
        SDL_EndGPUCopyPass(copyPass);
    }
    
    // render GUI
    {
        SDL_GPUColorTargetInfo color{};
        color.texture = swapTex;
        color.load_op  = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        color.mip_level   = 0;

        ImGui::Render();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
        SDL_GPURenderPass *rpass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);

        gui_render(cmd, rpass);
        SDL_EndGPURenderPass(rpass);
    }
    
    SDL_SubmitGPUCommandBuffer(cmd);
}

SDL_GPUDevice* graphics_getDevice() {
    return device;
}