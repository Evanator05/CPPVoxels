#include <iostream>
#include <stdexcept>

#include "i_graphics.h"
#include "i_video.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "i_gui.h"

#include "shaders/main.h"
#include "shaders/depth.h"
#include "shaders/upscaler.h"
#include "shaders/indexmap.h"

SDL_GPUDevice* device = nullptr;

// textures
typedef struct _RenderTextures {
    enum Type { HalfDepth, FullDepth, IndexMap, Display, Count };
    SDL_GPUTexture *tex[Count]{};

    SDL_GPUTexture*& halfResDepth() { return tex[HalfDepth]; }
    SDL_GPUTexture*& fullResDepth() { return tex[FullDepth]; }
    SDL_GPUTexture*& indexMap()     { return tex[IndexMap]; }
    SDL_GPUTexture*& display()      { return tex[Display]; }

    void init(Uint32 width, Uint32 height) {
        cleanup();
        // halfres depth
        SDL_GPUTextureCreateInfo createInfo{};
        createInfo.width = (Uint32)width/2;
        createInfo.height = (Uint32)height/2;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;

        halfResDepth() = SDL_CreateGPUTexture(device, &createInfo);
        if (!halfResDepth()) std::cerr << "Failed to create halfres depth Texture\n";

        // fullres depth
        createInfo = {};
        createInfo.width = (Uint32)width;
        createInfo.height = (Uint32)height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;

        fullResDepth() = SDL_CreateGPUTexture(device, &createInfo);
        if (!fullResDepth()) std::cerr << "Failed to create fullres depth Texture\n";
        
        // index map
        createInfo = {};
        createInfo.width = (Uint32)width;
        createInfo.height = (Uint32)height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R32G32_UINT;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;

        indexMap() = SDL_CreateGPUTexture(device, &createInfo);
        if (!indexMap()) std::cerr << "Failed to create fullres depth Texture\n";

        // display texture
        createInfo = {};
        createInfo.width = (Uint32)width;
        createInfo.height = (Uint32)height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;

        display() = SDL_CreateGPUTexture(device, &createInfo);
        if (!display()) std::cerr << "Failed to create Display Texture\n";
    }

    void cleanup() {
        for (int i = 0; i < Count; i++) {
            if (tex[i]) SDL_ReleaseGPUTexture(device, tex[i]);
            tex[i] == nullptr;
        }
    }
} RenderTextures;

typedef struct _ComputePipelines {
    enum Type {HalfDepth, Upscale, IndexMap, Display, Count };
    SDL_GPUComputePipeline *pipelines[Count]{};

    SDL_GPUComputePipeline*& halfResDepth() { return pipelines[HalfDepth]; }
    SDL_GPUComputePipeline*& upscaler()     { return pipelines[Upscale]; }
    SDL_GPUComputePipeline*& indexMap()     { return pipelines[IndexMap]; }
    SDL_GPUComputePipeline*& display()      { return pipelines[Display]; }

    void cleanup() {
        for (int i = 0; i < Count; i++) {
            if (pipelines[i]) SDL_ReleaseGPUComputePipeline(device, pipelines[i]);
            pipelines[i] == nullptr;
        }
    }

} ComputePipelines;

RenderTextures renderTextures{};
ComputePipelines computePipelines{};

int winw, winh;

void graphics_init() {
    SDL_GetWindowSize(window, &winw, &winh);

    initDevice();
    renderTextures.init(winw, winh);
    initComputePipeline();

    
    SDL_SetGPUAllowedFramesInFlight(device, 1);
    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    // Uncomment to uncap framerate
    //SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
}

void graphics_cleanup() {
    computePipelines.cleanup();
    renderTextures.cleanup();    

    if (device) SDL_DestroyGPUDevice(device);
}

void graphics_resize() {
    SDL_GetWindowSize(window, &winw, &winh);
    renderTextures.init(winw, winh);
}

void graphics_update(void) {
    drawFrame();
}

void initDevice() {
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!device) std::cerr << "Failed to create GPU device\n";
    SDL_ClaimWindowForGPUDevice(device, window);
}

void initComputePipeline() {
    // half res depth pass
    SDL_GPUComputePipelineCreateInfo createInfo{};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)depth_spirv;
    createInfo.code_size = std::size(depth_spirv)*sizeof(*depth_spirv);
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

    computePipelines.halfResDepth() = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!computePipelines.halfResDepth()) std::cerr << "Failed to create depth pass compute pipeline\n";

    // upscaler
    createInfo = {};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)upscaler_spirv;
    createInfo.code_size = std::size(upscaler_spirv)*sizeof(*upscaler_spirv);
    createInfo.entrypoint = "main";

    createInfo.num_readwrite_storage_textures = 2;
    createInfo.num_samplers = 0;
    createInfo.num_readonly_storage_textures = 0;
    createInfo.num_readonly_storage_buffers = 0;
    createInfo.num_readwrite_storage_buffers = 0;
    createInfo.num_uniform_buffers = 0;

    createInfo.threadcount_x = 8;
    createInfo.threadcount_y = 8;
    createInfo.threadcount_z = 1;

    computePipelines.upscaler() = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!computePipelines.upscaler()) std::cerr << "Failed to create upscaler compute pipeline\n";

    // index map
    createInfo = {};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)indexmap_spirv;
    createInfo.code_size = std::size(indexmap_spirv)*sizeof(*indexmap_spirv);
    createInfo.entrypoint = "main";

    createInfo.num_readwrite_storage_textures = 2;
    createInfo.num_samplers = 0;
    createInfo.num_readonly_storage_textures = 0;
    createInfo.num_readonly_storage_buffers = GPUBUFFERCOUNT;
    createInfo.num_readwrite_storage_buffers = 0;
    createInfo.num_uniform_buffers = 0;

    createInfo.threadcount_x = 16;
    createInfo.threadcount_y = 16;
    createInfo.threadcount_z = 1;

    computePipelines.indexMap() = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!computePipelines.indexMap()) std::cerr << "Failed to create compute pipeline\n";

    // main
    createInfo = {};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)main_spirv;
    createInfo.code_size = std::size(main_spirv)*sizeof(*main_spirv);
    createInfo.entrypoint = "main";

    createInfo.num_readwrite_storage_textures = 2;
    createInfo.num_samplers = 0;
    createInfo.num_readonly_storage_textures = 0;
    createInfo.num_readonly_storage_buffers = GPUBUFFERCOUNT;
    createInfo.num_readwrite_storage_buffers = 0;
    createInfo.num_uniform_buffers = 0;

    createInfo.threadcount_x = 16;
    createInfo.threadcount_y = 16;
    createInfo.threadcount_z = 1;

    computePipelines.display() = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!computePipelines.display()) std::cerr << "Failed to create compute pipeline\n";
}

void drawFrame(void) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    
    // half res depth pass
    {
        SDL_GPUStorageTextureReadWriteBinding rw{};
        rw.texture = renderTextures.halfResDepth();

        SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, &rw, 1, nullptr, 0);
        SDL_BindGPUComputePipeline(cpass, computePipelines.halfResDepth());

        // bind buffers
        SDL_BindGPUComputeStorageBuffers(cpass, 0, gpubuffers_getVoxelBuffers(), GPUBUFFERCOUNT);

        Uint32 localSize = 16;
        Uint32 groupsX = ((winw/2)+localSize-1)/localSize;
        Uint32 groupsY = ((winh/2)+localSize-1)/localSize;
        SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
        SDL_EndGPUComputePass(cpass);
    }

    // upscale to full res primary ray starting depths
    {
        SDL_GPUStorageTextureReadWriteBinding upscaleBindings[2]{};
        upscaleBindings[0].texture = renderTextures.halfResDepth();
        upscaleBindings[1].texture = renderTextures.fullResDepth();

        SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, upscaleBindings, 2, nullptr, 0);
        
        SDL_BindGPUComputePipeline(cpass, computePipelines.upscaler());

        Uint32 localSize = 8;
        Uint32 groupsX = (winw+localSize-1)/localSize;
        Uint32 groupsY = (winh+localSize-1)/localSize;
        SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
        SDL_EndGPUComputePass(cpass);
    }

    // clear visiblility buffer
    {
        
    }

    // full res voxel index/visible voxel pass + maybe some other per voxel information
    {
        SDL_GPUStorageTextureReadWriteBinding rw[2]{};
        rw[0].texture = renderTextures.indexMap();
        rw[1].texture = renderTextures.fullResDepth();

        SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, rw, 2, nullptr, 0);
        SDL_BindGPUComputePipeline(cpass, computePipelines.indexMap());

        // bind buffers
        SDL_BindGPUComputeStorageBuffers(cpass, 0, gpubuffers_getVoxelBuffers(), GPUBUFFERCOUNT);

        Uint32 localSize = 16;
        Uint32 groupsX = (winw+localSize-1)/localSize;
        Uint32 groupsY = (winh+localSize-1)/localSize;
        SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
        SDL_EndGPUComputePass(cpass);
    }

    // render voxel models
    {

    }

    // render particles
    {

    }
    
    // visible voxel lighting pass
    {

    }
    // main draw pass / combine color with lighting pass
    {
        SDL_GPUStorageTextureReadWriteBinding rw[2]{};
        rw[0].texture = renderTextures.display();
        rw[1].texture = renderTextures.indexMap();

        SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, rw, 2, nullptr, 0);
        SDL_BindGPUComputePipeline(cpass, computePipelines.display());

        // bind buffers
        SDL_BindGPUComputeStorageBuffers(cpass, 0, gpubuffers_getVoxelBuffers(), GPUBUFFERCOUNT);

        Uint32 localSize = 16;
        Uint32 groupsX = (winw+localSize-1)/localSize;
        Uint32 groupsY = (winh+localSize-1)/localSize;
        SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
        SDL_EndGPUComputePass(cpass);
    }

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