#include <iostream>
#include <stdexcept>

#include "i_graphics.h"
#include "i_video.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "i_gui.h"

#include "shaders/main.h"


SDL_GPUDevice* device = nullptr;
SDL_GPUTexture* displayTexture = nullptr;
SDL_GPUComputePipeline* pipeline = nullptr;

int winw, winh;

void graphics_init(void) {
    SDL_GetWindowSize(window, &winw, &winh);

    initDevice();
    initDisplayTexture();
    initComputePipeline();

    //SDL_SetGPUAllowedFramesInFlight(device, 3);
    //SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
}

void graphics_cleanup(void) {
    if (pipeline) SDL_ReleaseGPUComputePipeline(device, pipeline);
    if (displayTexture) SDL_ReleaseGPUTexture(device, displayTexture);
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

void initDisplayTexture() {
    SDL_GPUTextureCreateInfo createInfo{};
    createInfo.width = (Uint32)winw;
    createInfo.height = (Uint32)winh;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    displayTexture = SDL_CreateGPUTexture(device, &createInfo);
    if (!displayTexture) std::cerr << "Failed to create Display Texture\n";
}

void initComputePipeline() {
    SDL_GPUComputePipelineCreateInfo createInfo{};
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.code = (const Uint8*)main_spirv;
    createInfo.code_size = std::size(main_spirv)*sizeof(*main_spirv);
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

    pipeline = SDL_CreateGPUComputePipeline(device, &createInfo);
    if (!pipeline) std::cerr << "Failed to create compute pipeline\n";
}

void drawFrame(void) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    
    // half res primary depth pass

    // upscale to full res depth pass
    
    // full res voxel index/visible voxel pass + maybe some other per voxel information

    // visible voxel lighting pass

    // main draw pass / combine color with lighting pass
    SDL_GPUStorageTextureReadWriteBinding rw{};
    rw.texture = displayTexture;
    rw.mip_level = 0;
    rw.layer = 0;

    SDL_GPUComputePass* cpass = SDL_BeginGPUComputePass(cmd, &rw, 1, nullptr, 0);
    SDL_BindGPUComputePipeline(cpass, pipeline);

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

    if (swapTex) {
        SDL_GPUBlitInfo bi{};

        bi.source.texture = displayTexture;
        bi.source.x = 0;
        bi.source.y = 0;
        bi.source.w = winw;
        bi.source.h = winh;

        bi.destination.texture = swapTex;
        bi.destination.x = 0;
        bi.destination.y = 0;
        bi.destination.w = sw;
        bi.destination.h = sh;

        bi.load_op = SDL_GPU_LOADOP_DONT_CARE;
        bi.filter = SDL_GPU_FILTER_NEAREST;

        SDL_BlitGPUTexture(cmd, &bi);
    }

    
    // render GUI
    {
        SDL_GPUColorTargetInfo color{};
        color.texture = swapTex;

        /* Preserve your compute output */
        color.load_op  = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;

        /* Required defaults */
        color.clear_color = {0, 0, 0, 0};  // ignored because LOAD
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