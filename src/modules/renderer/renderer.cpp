#include "renderer.h"
#include "window.h"

#include <stdexcept>

#include "gui.h"
#include "vulkan/vulkan.h"
void InitDevice(SDL_GPUDevice*& device, SDL_Window* window)
{
    VkPhysicalDeviceFeatures features10{};
    features10.shaderInt64 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    features12.shaderInt8 = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;

    SDL_GPUVulkanOptions vulkanOptions{};
    vulkanOptions.vulkan_api_version = VK_API_VERSION_1_2;

    vulkanOptions.feature_list = &features12;

    vulkanOptions.vulkan_10_physical_device_features =
        &features10;

    SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetStringProperty(
        props,
        SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
        "vulkan"
    );

    SDL_SetBooleanProperty(
        props,
        SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,
        true
    );

    SDL_SetPointerProperty(
        props,
        SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,
        &vulkanOptions
    );

    device = SDL_CreateGPUDeviceWithProperties(props);

    SDL_DestroyProperties(props);

    if (!device)
        throw std::runtime_error(
            std::string("Failed to create GPU device: ") +
            SDL_GetError()
        );

    if (!SDL_ClaimWindowForGPUDevice(device, window))
        throw std::runtime_error(
            std::string("Failed to claim window: ") +
            SDL_GetError()
        );
}

void Renderer::Init() {
    Window &window = GetModule<Window>();

    InitDevice(device, window.GetWindow());

    SDL_SetGPUAllowedFramesInFlight(device, 1);
    SetVSync(false); // setting to false uncaps framerate
}

void Renderer::Process() {
    Window &window = GetModule<Window>();
    glm::ivec2 size = window.GetSize();
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

    SDL_GPUTexture *swapTex = nullptr;
    Uint32 sw = 0, sh = 0;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window.GetWindow(), &swapTex, &sw, &sh);
    swapchainTexture.CreateFrom(swapTex, glm::ivec2(sw, sh), 0, SDL_GetGPUSwapchainTextureFormat(device, window.GetWindow()));

    for (IExecutableResource *resource : executableResources) {
        resource->Execute(cmd);
    }

    for (ShaderPass *pass : shaderPasses) {
        pass->Execute(cmd);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

void Renderer::Shutdown() {
    for (IResource *resource : resources) {
        resource->Destroy();
        delete resource;
    }
    resources.clear();
    for (ShaderPass *shaderPass : shaderPasses) {
        shaderPass->Destroy();
        delete shaderPass;
    }
    shaderPasses.clear();

    if (device) SDL_DestroyGPUDevice(device);
}

void Renderer::SetVSync(bool enable) {
    Window &window = GetModule<Window>();
    SDL_SetGPUSwapchainParameters(device, window.GetWindow(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, enable ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
}

SDL_GPUDevice* Renderer::GetDevice() {
    return device;
}