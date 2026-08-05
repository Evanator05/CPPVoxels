#include "renderingdevice.h"

RenderingDevice::~RenderingDevice() {
    Destroy();
}

void RenderingDevice::Create() {

}
void RenderingDevice::Destroy() {
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
}

void RenderingDevice::Process() {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

    for (ShaderPass *pass : shaderPasses) {
        pass->Execute(cmd);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}