#include "renderer.h"
#include "window.h"

#include <stdexcept>

#include "gui.h"

#include "shaders/main.h"
#include "shaders/depth.h"
#include "shaders/upscaler.h"
#include "shaders/indexmap.h"

#include "shaders/test.h"

Renderer::Renderer(Engine *e) : EngineModule(e) {
    
}

void InitDevice(SDL_GPUDevice *&device, SDL_Window *window) {
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!device) throw std::runtime_error("Failed to create GPU device");
    SDL_ClaimWindowForGPUDevice(device, window);
}

void Renderer::Init() {
    glm::ivec2 size = GetModule<Window>().GetSize();

    GetModule<Window>().ResizedScreen.Bind(
        [this](glm::ivec2 size) {
            UpdateDisplayTextures(size);
        }
    );

    InitDevice(device, GetModule<Window>().GetWindow());

    SDL_SetGPUAllowedFramesInFlight(device, 1);
    SDL_SetGPUSwapchainParameters(device, GetModule<Window>().GetWindow(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    CreateDisplayTextures();
    CreateComputePipeline();

    // Uncomment to uncap framerate
    //SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
}

void Renderer::Process() {
    glm::ivec2 size = GetModule<Window>().GetSize();
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    for (ComputePass *pass : computePassOrder) {

        std::vector<SDL_GPUStorageTextureReadWriteBinding> rw_bindings(pass->readwrite_storage_textures.size());

        for (size_t i = 0; i < pass->readwrite_storage_textures.size(); i++) {
            rw_bindings[i].texture = pass->readwrite_storage_textures[i]->GetGPUTexture();
        }

        SDL_GPUComputePass *cpass = SDL_BeginGPUComputePass(cmd, rw_bindings.data(), rw_bindings.size(), pass->readwrite_storage_buffers.data(), pass->readwrite_storage_buffers.size());
        SDL_BindGPUComputePipeline(cpass, pass->GetPipeline());
        
        // bind buffers
        if (pass->samplers.size()) SDL_BindGPUComputeSamplers(cpass, 0, pass->samplers.data(), pass->samplers.size());
        if (pass->readonly_storage_textures.size()) SDL_BindGPUComputeStorageTextures(cpass, 0, pass->readonly_storage_textures.data(), pass->readonly_storage_textures.size());
        if (pass->readonly_storage_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, pass->readonly_storage_buffers.data(), pass->readonly_storage_buffers.size());
        if (pass->uniform_buffers.size()) SDL_BindGPUComputeStorageBuffers(cpass, 0, pass->uniform_buffers.data(),pass->uniform_buffers.size());

        Uint32 localSize = 16;
        Uint32 groupsX = ((size.x)+pass->threadcount.x-1)/pass->threadcount.x;
        Uint32 groupsY = ((size.y)+pass->threadcount.y-1)/pass->threadcount.y;
        SDL_DispatchGPUCompute(cpass, groupsX, groupsY, 1);
        SDL_EndGPUComputePass(cpass);
    }

    SDL_GPUTexture *swapTex = nullptr;
    Uint32 sw = 0, sh = 0;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, GetModule<Window>().GetWindow(), &swapTex, &sw, &sh);

    // copy image into swaptexture
    if (swapTex) {
        SDL_GPUTextureLocation sourceLoc{};
        sourceLoc.texture = displayTextures.at("display").GetGPUTexture();
        SDL_GPUTextureLocation destLoc{};
        destLoc.texture = swapTex;
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_CopyGPUTextureToTexture(copyPass, &sourceLoc, &destLoc, sw, sh, 1, false);
        SDL_EndGPUCopyPass(copyPass);
    }

    // render gui
    {
        SDL_GPUColorTargetInfo color{};
        color.texture = swapTex;
        color.load_op  = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;

        ImGui::Render();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
        SDL_GPURenderPass *rpass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);

        GetModule<GUI>().Render(cmd, rpass);
        SDL_EndGPURenderPass(rpass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

void Renderer::Shutdown() {
    for (Texture *texture : displayTextureOrder) {
        texture->DestroyTexture();
    }
    for (ComputePass *pass : computePassOrder) {
        pass->DestroyPipeline();
    }

    if (device) SDL_DestroyGPUDevice(device);

}

void Renderer::CreateDisplayTextures() {
    glm::ivec2 size = GetModule<Window>().GetSize();
    Texture& display = displayTextures.try_emplace(
        "display",
        device,
        size, 
        SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM).first->second;
    display.CreateTexture();
    displayTextureOrder.push_back(&display);
}

void Renderer::UpdateDisplayTextures(glm::ivec2 size) {
    displayTextures.at("display").size = size;
    displayTextures.at("display").CreateTexture();
}

void Renderer::CreateComputePipeline() {
    ComputePass& display = computePasses.try_emplace("display", device).first->second;
    display.threadcount = {16, 16, 1};
    display.spirv = test_spirv;
    display.spirv_size = std::size(test_spirv);
    display.readwrite_storage_textures.push_back(&displayTextures.at("display"));
    display.CreatePipeline();
    computePassOrder.push_back(&display);
}

SDL_GPUDevice* Renderer::GetDevice() {
    return device;
}