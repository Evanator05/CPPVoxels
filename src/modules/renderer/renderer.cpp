#include "renderer.h"
#include "window.h"

#include <stdexcept>

#include "gui.h"

#include "shaders/main.h"
#include "shaders/depth.h"
#include "shaders/upscaler.h"
#include "shaders/indexmap.h"

#include "shaders/test.h"

void InitDevice(SDL_GPUDevice *&device, SDL_Window *window) {
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!device) throw std::runtime_error("Failed to create GPU device");
    SDL_ClaimWindowForGPUDevice(device, window);
}

void Renderer::Init() {
    Window &window = GetModule<Window>();

    // window.ResizedScreen.Bind(
    //     [this](glm::ivec2 size) {
    //         UpdateDisplayTextures(size);
    //     }
    // );
    //glm::ivec2 size = window.GetSize();

    InitDevice(device, window.GetWindow());

    SDL_SetGPUAllowedFramesInFlight(device, 1);
    SetVSync(true); // setting to false uncaps framerate
}

void Renderer::Process() {
    Window &window = GetModule<Window>();
    glm::ivec2 size = window.GetSize();
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

    SDL_GPUTexture *swapTex = nullptr;
    Uint32 sw = 0, sh = 0;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window.GetWindow(), &swapTex, &sw, &sh);
    swapchainTexture.CreateFrom(swapTex, glm::ivec2(sw, sh), 0, SDL_GetGPUSwapchainTextureFormat(device, window.GetWindow()));

    for (ShaderPass *pass : shaderPassOrder) {
        pass->Execute(cmd);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

void Renderer::Shutdown() {
    for (ShaderPass *pass : shaderPasses) {
        pass->Destroy();
    }
    for (Texture *texture : textures) {
        texture->Destroy();
    }
    if (device) SDL_DestroyGPUDevice(device);
}

void Renderer::SetVSync(bool enable) {
    Window &window = GetModule<Window>();
    SDL_SetGPUSwapchainParameters(device, window.GetWindow(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, enable ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
}

SDL_GPUDevice* Renderer::GetDevice() {
    return device;
}